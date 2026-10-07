# FD2.EXE 逆向测绘 / 源码转译地图（RE_MAP）

> 由 **ida MCP** 生成，与 `port/PROGRESS.md`（宿主/加载层）、`FD2_analysis.md`（资源格式）三足鼎立。
> 所有地址为线性地址（obj0=0x10000 / obj1=0x50000 / obj2=0x60000），已与 Ghidra 实证一致。
> 结论凡未经运行验证的均标注"待确认"。**第二个游戏《炎龙外传》FDPS.EXE 的地图见 `re/FDPS_MAP.md`**
> （90 条 AIL 入口表、定时器族、spawn FD.EXE 流程；IDA 库 `…\\FDPS\\FDPS.EXE.i64`）。
> **FDPS 已冻结（2026-10-05 用户决定，不再支持），该图仅作存档不再更新。**

---

## 0. IDA 环境（下轮直接复用）

| 项 | 值 |
|---|---|
| IDA | IDA Professional 9.5 + Hex-Rays (x86)，`E:\Dev\IDA Professional 9.5` |
| MCP server | `ida`（`use_capability` → `mcp-tool:ida/*`） |
| 数据库 | `E:\FD2\FD2.EXE` → 已保存 `E:\FD2\FD2.EXE.i64`，instance_id = `8fe1e266775b`（GUI 后端） |
| 产物 | `port/re/funcmap.csv`（1359 个函数全表）、`E:\FD2\FD2.i64` |

标准调用序：`ida/open_database(path)` → `ida/execute_python(code)`（`db`、`ida_domain` 全局可用）→ `ida/save_database`。

**API 陷阱（IDA 9.5 实测）**：
- 没有 `idc.decompile`，用 `ida_hexrays.decompile(ea)`；先 `ida_hexrays.init_hexrays_plugin()`。
- `idautils.Strings()` 直接返回 list（无 `.setup()`）；`idautils.Entries()` 返回 4 元组。
- 变量在同实例会话间持久（`ail_map` 等可跨调用复用）。
- 批量结果**写文件**（`open(r'E:\FD2\port\re\...')`）而不是返回，避免吃上下文。

---

## 1. 加载验证（IDA vs Ghidra，逐项一致）

| 事实 | PROGRESS.md (Ghidra) | IDA |
|---|---|---|
| 格式 | LE @文件 0x27ACC | `Linear Executable (LE)` ✅ |
| obj0 | 0x10000, vsize 0x3EF29 | `cseg01` 0x10000..0x4F000 ✅ |
| obj1 | 0x50000, vsize 0x56B0 | `dseg02` 0x50000..0x556B0 ✅ |
| obj2 | 0x60000, vsize 0x34D2 | `dseg03` 0x60000..0x64000 ✅ |
| 入口 | obj0+0x2CCB4 = **0x3CCB4** | entry `start` = **0x3CCB4** ✅ |

fixup 已被 IDA 应用（main 反编译的字符串/函数交叉引用全部解析正确）。启动链：
`start(0x3CCB4)` → `__CMain(0x4609B)` → **`main(0x25BF4)`**。

---

## 2. 函数分区（全表：`port/re/funcmap.csv`）

| zone | 数量 | 地址范围 | 含义 / 转译策略 |
|---|---|---|---|
| `game` | **569** | 0x10000..0x37000 | **游戏本体 —— 转译对象** |
| `crt_sym` | 150 | 散布 0x37000+ | Watcom CRT（`malloc`/`fopen`/`int386`/`sbrk`…）**不转译**，映射到现代 libc/宿主 |
| `ail` | 51 | 0x37D3E 起 | Miles AIL V3.02，**整体打桩替换**（见 §3） |
| `lib_nosym` | 589 | 0x37000+ | CRT/stdio/math/AIL 的 static 内部 + 游戏侧工具库（0x4D000..0x4F000 段），**待归类** |

CSV 列：`addr,size,name,zone,flags,callers_game,callers_lib,data_xrefs,str_refs`
flags 粗筛：`gfx_A0000`（字节含 `00 00 0A 00`）172 个 —— **含误报，需抽样核实**；`swi21`/`swi31` 只出现在 lib 区。

### ★ 关键结构结论

1. **游戏区 569 个函数中零条 `int` 指令**（`CD xx` 全部在 lib 区）。
   游戏逻辑与 DOS/BIOS 的交互**全部经由 CRT 包装**（`int386()`/`sbrk`/`fopen`/`read`…）。
   ⇒ 分层转译成立：游戏层是纯计算 + `0xA0000` 显存访问；平台层只需给 CRT 函数提供 Win32 实现（`port/src/dos.c` 已在做）。
2. 高密度 4KB 块（疑似源文件边界）：`0x35000`(48) `0x36000`(39) `0x34000`(35) `0x33000`(27) ⇒ 0x33000..0x37000 约 149 函数为一个大模块（疑渲染/动画核心，待确认）；`0x20000..0x24000` 共 96 个为另一模块。
3. 函数密度按块见 `funcmap.csv`，可自行 `Import-Csv | Group-Object`。
4. **游戏自带工具库的精确边界 = `0x4DED4..0x4EF29`**（obj0 末尾，`callers_game > 0`）。
   原先按地址粗分的 `lib_nosym` 里，`0x4D031..0x4DED4` 实为 CRT 数学/stdio
   （`_matherr`/`_FtoS`/`strtod`/`fputs`/`frexp`…）；**只有 0x4DED4 之后才是游戏代码**：
   RLE 解压（`sub_4E98D`）、随机数（`sub_4EBE3`，`ROL16(word_627B8-28652)`）、
   BIOS/键盘封装（`sub_4E381`，`BDA[0x41C]=BDA[0x41A]`）、24×24 图元处理（`sub_4E22A`）等，
   约 60 个函数 —— **这些属于源码转译范围**。
5. **两张 0x50 字节记录表**：`dword_53A45`（角色/临时表，`dword_53BEB` 条）与
   `dword_53BF7`（持久队伍表，`dword_53BFB` 条 = 下一个 append 索引）。队伍表的唯一写入者
   是 `0x1145A`（按 8 个物品格重算 +48..+4E 衍生值）、`0x11506`（按 +8 身份把角色记录整笔
   抄回队伍记录）、`0x112A5`（JOIN：按 `tbl_61DA1` 默认 + `tbl_620A1` 成长构造并 append）。
   转译接入后 `funcs_25E23`（`0x51DE9`）状态分派表的 5 个表项（`0x22EF6`/`0x231BC`/
   `0x23790`/`0x2389F`/`0x23E39`）已经**源码化接入**（`game/scene.c` 的
   `scene_state_00/03/10/12/18`，`scenecheck`，见 `docs/rounds/30-scene-states.md`）；
   表内其余 20 项及其依赖尚未转译（`docs/rounds/29-unit-roster.md` §59.5）。

---

## 3. AIL 库边界（51 个函数已批量命名）

由 `"AIL_xxx()\n"` trace 格式串的交叉引用反推命名，起点 `0x37D3E AIL_startup`，
含 `AIL_shutdown / AIL_set_preference / AIL_{get,set}_real_vect / AIL_call_driver /
AIL_*_timer* / AIL_install_DIG_INI / AIL_allocate_sample_handle / AIL_*_sample*` 等。

- main 的音频初始化链：`sub_3702F(28)` → `AIL_startup` → `?` → `AIL_install_DIG_INI(0x3908B)` → `AIL_allocate_sample_handle(0x392D0)` ×2。
- **替换策略**（见 `docs/AUDIO.md`）：51 个 `AIL_*` 入口全部打桩 → "成功但静音" 或转接现代音频后端；`*.DIG/*.MDI` 已在宿主报"文件不存在"。
- 无 trace 的 AIL 内部函数尚未分离（散在 `lib_nosym`），打桩以 51 个导出入口为界即可。

### 3.1 游戏实际只调用 16 个入口（宿主替换清单）

| 入口 | 地址 | 游戏调用者 |
|---|---|---|
| `AIL_startup` | 0x37D3E | `main` |
| `AIL_shutdown` | 0x37ED8 | `main`, `sub_33FAF` |
| `AIL_install_DIG_INI` | 0x3908B | `main` |
| `AIL_install_MDI_INI` | 0x3AA72 | `main` |
| `AIL_allocate_sample_handle` | 0x392D0 | `main` ×2 |
| `AIL_allocate_sequence_handle` | 0x3ACA3 | `main` |
| `AIL_init_sample` | 0x39521 | `sub_25A96`, `sub_25B45` |
| `AIL_set_sample_address` | 0x39694 | `sub_25A96`, `sub_25B45` |
| `AIL_set_sample_loop_count` | 0x39AAE | `sub_25A96`, `sub_25B45` |
| `AIL_start_sample` | 0x39798 | `sub_25A96`, `sub_25B45` |
| `AIL_stop_sample` | 0x39805 | `sub_25A96`, `sub_25B45` |
| `AIL_init_sequence` | 0x3ADF5 | `sub_25977` |
| `AIL_start_sequence` | 0x3AEEE | `sub_25977` |
| `AIL_stop_sequence` | 0x3AF5B | `sub_25977` |
| `AIL_set_sequence_volume` | 0x3B124 | `sub_25977`, `sub_1728C` |
| `AIL_set_sequence_loop_count` | 0x3B1A6 | `sub_25977` |

⇒ 这 16 个入口被宿主改写为 `jmp`（`port/src/ail.c`，见 docs/AUDIO.md §11）。
**游戏从不调用 `AIL_set_sample_type` / `_playback_rate` / `_volume` / `_pan`** ⇒ 采样走 AIL
默认值（8 位无符号单声道、11025 Hz），已由运行时字节统计证实。

其中两条音效包装（每个都有两个 `AIL_allocate_sample_handle` 句柄之一）：

```c
/* sub_25A96(…, a5=音效包, a6=索引, a7=循环次数)；sub_25B45 同构 */
v8  = a5 + 4*a6;  v10 = *(DWORD*)(v8+6) + a5;      /* 样本地址 */
v9  = *(DWORD*)(v8+10) - *(DWORD*)(v8+6);          /* 样本长度 */
AIL_init_sample(h); AIL_set_sample_address(h, v10, v9);
AIL_set_sample_loop_count(h, a7); AIL_start_sample(h);
```
音效包 = `dword_53EEC` = **FDOTHER.DAT 资源 31**（`[6 字节头][u32 偏移表][PCM]`）。

### 3.2 AIL 内部关键函数（将来若要"让 AIL 自己跑"时的参考）

| 地址 | 标识 | 作用 |
|---|---|---|
| `0x382E9` | `AIL_call_driver` | 驱动回调入口（无 16 位驱动时的关键断点） |
| `0x41B84` | 数字音脚本解释器 | 分发 `AIL_set_sample_address/_playback_rate/_type/_start_sample`（case 0/1/4/6/7/8/9） |
| `0x42520` | **XDIR 目录 walker** | 下钻 `FORM`/`CAT` 直到块类型为 `XMID`，再取第 `sequence_num` 个 `FORM XMID` 子块（0-based） |
| `0x43160` | sequence 状态重置 | 通道/状态初始化（`a1[24]=4`、`a1[27]=8000000` 等） |
| `0x443D0` | **`AIL_init_sequence` 真身** | 遍历 `FORM/CAT` 收集 `TIMB`/`RBRN`/`EVNT` 指针；`a1[4]` = EVNT |
| `0x44790` | **`AIL_start_sequence` 真身** | 设置事件流指针 `a1[5] = EVNT + 8`（数据起点） |
| `0x3ACA3` | `AIL_allocate_sequence_handle` | 音乐侧句柄分配 |

**XMIDI 事件流解析规则**（delta 为 0 时省略、running status 数据、**60 ticks/beat 且墙钟时间由
`FF 51 03` tempo 换算**、本作几乎无 note-off）见 docs/AUDIO.md §11 —— 这是播放音乐必须知道的，
按标准 SMF 处理会得到明显错误的时长与卡音。

---

## 4. 核心函数档案（转译优先级依据）

| 地址 | 标识 | 作用（证据） | 转译优先级 |
|---|---|---|---|
| `0x25BF4` | `main` | 主流程：AIL 初始化 → `sub_111BA` 加载 8 组 DAT → `int386(0x10)` 设 0x13 模式 → `rand()%256` 次 `sub_4EBE3` → 主状态机循环 → `int386(0x10)` mode 3 退出 | ★★★ 骨架 |
| `0x111BA` | 资源加载器（**已转译** `src/game/res.c`，对拍 160 例） | cdecl 3 参 `res_load(filename, oldbuf, index)`；LMI 容器 `+6` 起 count+1 个 u32 偏移；写 `dword_53BFF`；132 调用点 | ★★★ |
| `0x15F84` | 脚本 VM（PROGRESS 中 `FUN_00015f84`）—— **已转译 `src/game/vm.c`（§39，`vmcheck` 5512 例）** | 1380 字节，**9 个 cdecl 栈参数**（不是 `__usercall` 14 寄存器参数：入口 `push <帧大小>; call 0x3702F` 是 Watcom 栈探针 `_chkstk`，IDA 把它的原型当成了函数的 ABI），局部状态机 | ★★★ |
| `0x25EBB` `0x117E7` `0x22E5C` `0x26152` | 主状态机 | main 循环核心；含局部函数指针表 `funcs_25E23[]`/`funcs_25E3A[]`（表地址待从反汇编 `lea` 提取） | ★★★ |
| `0x25977` | `play_bgm`（换曲入口，**已转译** `game/bgm.c`，对拍 6000 例） | 32 调用点；res_load(FDMUS.DAT)+AIL 序列族+DPMI lock | ★★ |
| `0x4E98D` | **RLE 行解压 + blit**（已确认） | 序言 `ESI=src; w=[esi]; h=[esi+2]; EDI = a4 + a3*a5 + a2`（a4=目标基址、a5=pitch、a2/a3=偏移），每个扫描行按 token 做 `rep stosb` / `rep movsb` / 跳过，行末 `EDI += a5 - w`；共 39 个调用点（如 `sub_10652` 解 FDOTHER 资源到 `malloc` 缓冲、`sub_1F894` 解说 0xA0000 帧缓冲） | ★★★ |
| `0x373CA` | stdio 写核心 | 466 次游戏调用，FILE+12 flags、`_ioalloc` ⇒ 属 CRT，**不转译** | — |
| `0x10010` | 存档 | 1552B，引用 `FD2.SAV`/`FD2.TMP` | ★★ |
| `0x10B4E` | FDICON.B24 加载 | 引用 `File 'FDICON.B24' error` | ★★ |
| `0x11019`/`0x127E0`/`0x127A9` | 头像/图标图集与绘制（**已转译** `game/dlg.c`，`mapcheck` 97000 例） | `0x11019` 把每个图标资源的 12 张子图读进 `*(0x53A61)`（**32 位偏移表在基址**，表长 12×已载资源数）；`0x127E0` 按记录字段选帧（索引 `mode+12*p[2]+3*p[3]`，每 BIOS tick 翻 `dword_53A04`，`p[5]` bit7 选 `sprite24_ramp24`/`plain`）；`0x127A9` 扫全部未标记记录后 `map_refresh_records` | ★★ |
| `0x1B722`/`0x344F2`/`0x1BB8C`/`0x1B8E7` | 角色记录「8 槽」字段访问器（**已转译** `game/rec.c`，`reccheck` 32010 例） | 8 个 2 字节槽在记录 +10：`[state, value]`，state bit7=空。`0x1B722` 读值字节 +11+2·slot；`0x344F2` 闭区间置 +52 低半字节（`and 0F0h; or cl` **不截断** value）；`0x1BB8C` 占首空槽（返回 1/-1）；`0x1B8E7` memmove 左移删除、末槽 `rec[24]=0x80`。基址 `dword_53A45 + 80u*index` | ★★ |
| `0x20421` | ANI.DAT 加载 | 引用 `ANI.DAT` | ★★ |
| `0x4E381` | 清键盘缓冲 | `MEMORY[0x41C]=MEMORY[0x41A]`（BDA） | ★（宿主 BDA 已支持） |
| `0x4EBE3` | 随机表滚动 | `ROL16(word_627B8-28652)` | ★ |
| `0x3702F` | 库公共 thunk？ | 538 lib + 16 game 调用，`_InterlockedExchange` 包装 —— **待确认**（lib 层，不转译） | — |

> **转译进展（截至第 33 轮）**：`0x4E98D`+`0x4E8D3` RLE → `rle.c`（1900）；图形 blit 工具族
> `0x4EC7C..0x4EEE0` → `gfx.c`（1450）；0xC0-RLE 文本 blit `0x4EBFF/0x4EC31/0x4EBAB` → `rle2.c`（1200）；
> 24×24 精灵 RLE 族 → `sprite24.c`（2100）；字节/调色板工具 → `util.c`（2200）；地形代价洪泛/寻路
> `0x4E390..0x4E751` → `path.c`（1000）；表访问器 `0x4E7DD..0x4E8BC` → `tables.c`（4528）；
> 资源加载 `0x111BA` → `res.c`（160）；对话框辅助 `0x16559/0x16E24` → `dlg.c`（800）；开框/收框动画
> `0x165AC/0x16B43/0x168B6/0x1685C` → `dlg.c`（boxcheck 240）；等键+嘴型 `0x16C57` → `dlg.c`（keycheck 100）；
> 角色记录表 `0x34894/0x12C60` → `rec.c`（28739），8 槽字段访问器
> `0x1B722/0x344F2/0x1BB8C/0x1B8E7` → `rec.c`（`reccheck` 32010）；**系统服务** `0x17AA9` 等 N 个 BIOS tick /
> `0x25A96` 播 PCM 音效 → `svc.c`，打字机步进 `0x164E8` → `dlg.c`（typecheck 1176）；
> 淡变 `0x11D40/0x1F882/0x1F525` → `fade.c`（4000）；脚本 VM `0x15F84` → `vm.c`（5512）；
> 地图/动画/头像绘制（`map.c`/`anim.c`/`dlg.c` 的 `0x126F7`/`0x1297D`/`0x187D6`/
> `0x12AC6`/`0x129EC`/`0x127E0`/`0x127A9` 等）→ `mapcheck`（97000）；
> 其中 **85 个经 `src/repl.c` 接入运行中的游戏**（机器码逐字节对拍 + `regress` 8/8，docs/rounds/*.md（按轮次分卷，见 docs/INDEX.md）§19..§58）。

游戏侧高频依赖（`lib_nosym`，需归类确认属于谁）：`0x4E381(15/64)`、`0x4EBE3(28/40)`、`0x4DF4C(56/32)`、`0x4E22A(114/13)`、`0x4E31C(101/15)` —— 0x4D000..0x4F000 段像**游戏自带工具库**（位流、24×24 图元、BIOS 封装），优先归类。

**0x4DED4..0x4E8C0 工具库地形图（第 22/23 轮）**：
`0x4DED4..0x4DEDA` 表访问器；`0x4DEEC/0x4DF09/0x4DF28/0x4DF4C` 字节/调色板变换（含 ROL 混淆）——
**已转译** `src/game/util.c`；`0x4DF84..0x4E29C` **24×24 精灵 RLE 族 7 变体**（已转译
`src/game/sprite24.c`）；`0x4E310/0x4E31C` 计时器读取 + 调色板动画（写 DAC 端口）；
`0x4E381` 清键盘缓冲（BDA）；`0x4E390..0x4E751`
连通性/BFS 簇（`byte_60068/69` 网格 + 四方向递归，**已确认为地形代价洪泛+寻路**，
已转译 `src/game/path.c`，对拍 1000 例）；
`0x4E795` 掩码重着色（已转译 `util_mask_recolor`）；`0x4E7DD..0x4E8BC` 系列表访问器
（`&unk_XXXX + 步长*i`，已转译 `src/game/tables.c`）。**该区段至此全部转译完成。**

---

## 5. 转译路线（路线 C 的源码侧）

| 阶段 | 内容 | 验证 |
|---|---|---|
| 0 ✅ | IDA 数据库 + 测绘（本文档 + funcmap.csv） | 与 Ghidra 逐项一致 |
| 1 | 模块归类：把 569 `game` + 0x4D000..0x4F000 段按调用图/字符串聚成模块；提取数据结构（全局 `dword_53xxx` 状态区、obj2 数据） | 交叉引用人工核对 |
| 2 | **叶子模块先行**：DAT 解码器（BG/SHAP/ANI/RLE，格式已知于 FD2_analysis.md）→ 独立 C + 测试样本对拍 | 与原始解压输出逐字节 diff |
|   | ✅ **RLE 已完成**（第 19 轮）：`src/game/rle.c` + `rlecheck` 机器码对拍 1900 例（docs/rounds/02-translation-toolkit.md §19） | 对拍判据已建立，可复用于后续模块 |
|   | ✅ **图形 blit 工具族已完成**（第 21 轮）：`src/game/gfx.c`（save/restore rect、block/透明 blit、16×16 字形、scanline 重排）+ `gfxcheck` 对拍 1450 例（docs/rounds/02-translation-toolkit.md §21） | 同 §19.4 |
|   | ✅ **24×24 精灵 RLE 族已完成**（第 22 轮）：`src/game/sprite24.c`（7 变体）+ `sprite24check` 对拍 2100 例（docs/rounds/02-translation-toolkit.md §22） | 同 §19.4 |
|   | ✅ **字节/调色板工具已完成**（第 23 轮）：`src/game/util.c`（6 函数）+ `utilcheck` 对拍 2200 例（docs/rounds/02-translation-toolkit.md §23） | 同 §19.4 |
|   | ✅ **地形代价洪泛/寻路已完成**（第 24 轮）：`src/game/path.c`（2 入口 + 7 内部）+ `pathcheck` 对拍 1000 例（docs/rounds/03-tables-and-plumbing.md §24） | 同 §19.4 |
|   | ✅ **资源加载器已完成**（第 25 轮）：`src/game/res.c`（原 `0x111BA`）+ `rescheck` 对拍 160 例；同轮建立 **CRT 重定向对拍术**（docs/rounds/03-tables-and-plumbing.md §25.2） | 同 §19.4 + CRT 替换 |
|   | ✅ **表访问器已完成**（第 27 轮）：`src/game/tables.c`（11 个）+ `tablescheck` 对拍 4528 例（docs/rounds/03-tables-and-plumbing.md §27） | 同 §19.4 |
|   | ✅ **0xC0-RLE 文本 blit 已完成**（第 28 轮）：`src/game/rle2.c`（3 变体 + 共享解码器）+ `rle2check` 对拍 1200 例（docs/rounds/04-dialog-and-ui.md §28） | 同 §19.4 |
|   | ✅ **对话框辅助已完成**（第 29 轮）：`src/game/dlg.c`（`0x16559` 贴 DATO 子图 / `0x16E24` 文本上滚）+ `dlgcheck` 对拍 800 例（整帧 VGA 对比，docs/rounds/04-dialog-and-ui.md §29） | 同 §19.4 |
|   | ✅ **开框/收框动画已完成**（第 30 轮）：`src/game/dlg.c`（`0x165AC` 开框+人像滑入 / `0x16B43` 收框 / `0x168B6` 5 阶段贴框 / `0x1685C` 贴瓦片）+ `boxcheck` 对拍 240 例（VGA + 5 段快照 + 事件序列 + 每次 delay 抓帧，docs/rounds/04-dialog-and-ui.md §30） | 同 §19.4；app-level 写法：全局留原地址、堆/时序服务留原机器码 |
| 3 | 图形 blit/调色板（`gfx_A0000` 粗筛集，先精化名单） | fd2host 显示对拍 |
| 4 | 主状态机 + 脚本 VM | 逐步替换法：机器码 vs 转译 C 逐函数对拍（**待确认**可行性） |
| 5 | CRT/平台层 → Win32（`dos.c` 已有大半）+ AIL 打桩 | host.log 行为等价 |
| 6 | x86-64 构建，脱离 32 位宿主 | 全流程跑通 |

**转译产物建议**：`port/src/game/*.c`，一模块一文件，地址/函数名注释保留（`// 0x15F84`），便于回查 IDA。

---

## 6. 平台层覆盖扫描（第 12 轮，2026-10-05，ida MCP 产物）

> 用 ida MCP 回答一个问题：**宿主还缺哪些 `INT` 服务**。三份产物都在 `port/re/` 下，
> 可直接复用，不必重跑：
>
> | 文件 | 内容 |
> |---|---|
> | `int21_ah_used.txt` | 每个 `int 21h` 站点回看 `mov ah,imm` 得到的 **AH 直方图 + 所属函数** |
> | `int_sites_all.txt` | obj0 `0x10000..0x4F000` 的 `CD xx` **裸字节**扫描（非 IDA head） |
> | `int386_callers.txt` / `int_vec_all.txt` | `int386()` 的**常量向量**调用点 |
> | `file_apis.txt` / `sub_19DF7_save.c` / `file_strings.txt` | `sopen`/`unlink`/`remove` 反编译、存档函数、`FD2.SAV`/`FD2.TMP` 交叉引用 |

### 6.1 INT 21h：游戏/CRT 实际会发的 AH vs 宿主已实现

静态扫出的 AH（十进制标签 → 十六进制）：`01 05 08 25 2A 2C 30 35 3C 3D 3E 3F 40 41 42 44 48 49 4A 4C`
（另有 12 个站点是运行时装载的 AH，未计入）。

| AH | 含义 | 宿主状态 |
|---|---|---|
| 25/2A/2C/30/35 | 设/取向量、日期、时间、版本 | ✅ 已实现 |
| 3D/3E/3F/40/42/44 | 打开/关闭/读/写/lseek/IOCTL | ✅ 已实现（`40` 的 `CX=0` 截断是第 12 轮补的） |
| 48/49/4A/4C | 分配/释放/重分配/终止 | ⚠️ `49`/`4A` 是**空操作返回成功**（账本只增不减，待改） |
| 01/05/08 | 控制台输入类 | ✅ 返回"无键"（`06/07/08/0B` 同） |
| **3C** | **创建/截断** | ✅ **第 12 轮补上**（缺了会崩，见 docs/rounds/01-platform-and-tooling.md §12.1） |
| **41** | **删除** | ✅ 第 12 轮补上（游戏无调用点，属兜底） |
| 43 / 4D / 4E / 4F / 56 | 属性 / 返回码 / 查找 / 改名 | — **静态确认无调用点，不实现**（除非出现新的 `UNHANDLED INT21`） |

### 6.2 "鼠标 INT 33h" 结论：游戏不用鼠标

| 检查 | 结果 |
|---|---|
| obj0 `CD xx` 裸扫 | `int 0x33` 仅 `0x469E1` 一处，在 DOS/4GW 的 `int NN; ret` **桩表**（0x46948 起每 3 字节一项）内 |
| `int386()` 常量向量 | 只有 `0x10` / `0x16` / `0x31` |
| `push 0x33`（3 处，`push33_sites.txt`） | 是 `sub_1366A(…,51)` / `sub_34894` 的**标志位索**（相邻传 50/52/53），不是中断号 |
| 运行期 `host.log` | `int 33` 从未出现 |

⇒ "鼠标 `INT 33h` 接真实状态" 这条旧计划项**已划掉**（判定过程见
`docs/rounds/01-platform-and-tooling.md` §12.3）；宿主 `int33()` stub 保留。

> **方法论坑**：`CD xx` 裸扫必然是噪声（本作连 `CD 00`…`CD FF` 每种字节都有），必须按
> **指令边界 + 所处区段**（桩表 / CRT / 游戏区）分类后才可用。参见 docs/PITFALLS.md §8-25、§12.3。

---

## 7. 状态与下轮入口

**宿主侧（2026-10-05）**：docs/PITFALLS.md §6 的卡点已修复 —— 根因是 INT 21h 的语义
（`AH=42` 的 CX:DX/DX:AX 约定、`AH=48` 读全 EBX）加上需要预映射 1 MiB 内的实模式区。
现在游戏能原生持续渲染开场动画（30 s / 960 帧无崩溃），画面颜色也已修正
（6 位 DAC 伸展 + BGRA 通道序）。**第 12 轮**又补完了文件服务（`AH=3C/41` + `AH=40 CX=0` 截断，
`port/regress.ps1` 8/8 PASS）并关掉了"鼠标"项。宿主现状见 `PROGRESS.md` §0、§7、§12。

1. 游戏工具库的边界已确定（§2 第 4 条）；`lib_nosym` 里剩余的是 CRT/AIL 内部函数，优先级低。
2. 提取 `main` 状态机两张函数指针表（`funcs_25E23`/`funcs_25E3A`）的真实地址与项。
3. 精化 `gfx_A0000` 名单（当前是字节粗筛，含误报）。
4. 全局状态区 `dword_53A00..0x53F00` 的结构还原（`main` 已见约 20 个成员）。
5. ~~**下一批源码转译目标**：★★★ `sub_15F84`（文本/脚本渲染器）~~ **已完成（§39，2026-10-06）**：
   `src/game/vm.c` + `src/vmcheck.c`（12 个被调函数全部桩化、比完整事件序列 + 全局 + 返回值，
   **5512 例 0 失败**），接入分组 `vm`（接入 50 → 51）。三个测绘结论记在
   `docs/rounds/09-vm.md` §39.2：① ABI 是 9 个栈参数（上文第 5 条旧结论已更正）；
   ② `case -1` 的 `JUMPOUT(0x15309)` 是与 `sub_15055` 共享的尾声 ⇒ `return cur`；
   ③ EDI 是跨 opcode 持久的“当前记录”指针，唯一不可复现的 `-17/39` 首发路径留了运行期探针。
6. **官方逆向知识库**：`port/docs/`（已 curate 到 9 MB/274 文件，见 `docs/KEEP.md`）可作语义线索；
   但**它是另一个 FD2.EXE build**（md5 `b97caf22…`，非本项目 `a6e341a8…`），地址/常量/指令
   一律以 `E:\FD2\FD2.EXE.i64` 复核。详见 docs/rounds/02-translation-toolkit.md §21.1/§22.1。
6. **平台侧遗留**（第 12 轮收尾清单）：`AH=49/4A` 改成真释放；"首次保存"与"存档变小截断"
   两条路径实测；深层路径（战斗/地图）出现新 `UNHANDLED INT21` 时按 §6.1 表补齐。
