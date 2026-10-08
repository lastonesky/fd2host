# 源码转译（路线 C 主体）总览

> 这是 `PROGRESS.md` 第 19–33 轮的**方法与现状提炼**；每一轮的完整病历（为什么不这么写、
> 踩了什么、判据是什么）在 `docs/rounds/*.md`，细节以轮次文件为准。

**目标**：把 `FD2.EXE` 的机器码整体还原成**人类会写的 C**（不像反编译器输出），
最终产出可编译 x86-64 的引擎。前提是**程序必须一直能运行**：每个模块先接入跑起来，
再谈优雅。

> ⚠ **进度基线**：`re/funcmap.csv` 共 **1359** 个函数，已完成接入 **261 个 ≈ 19.2%**（清单见 §4），
> **其余 ~88% 仍是原始机器码在宿主进程里执行**。
> 这也是路线 C 中期必须用 32 位宿主（Linux `-m32`）的原因；**全部转译完就不再需要 `-m32`**。

## 1. 转译流程（每轮都走这七步）

> **批量节奏（2026-10-08 起）**：选目标时按**同表/同族的依赖闭合簇**一次选 ~30 个，
> 一批写完再验；验证用 `*check --only=addr,...` **每 3-5 个跑一次**，失败就缩到 **1-2 个**定位、
> 改完回到 3-5。宿主集成若挂但对拍全过，用 `FD2_REPL_SKIP=0x..,0x..` 二分（无需重建）。
> 第 39/40 轮（`docs/rounds/39-funcs1199c-batch2.md`、`40-funcs1199c-batch3.md`）是首个/第二批 30/28 函数批次。
>
> 1. **选模块**：叶子优先（工具层/解码器 → UI → VM），依赖越少越好 → `re/RE_MAP.md` 有转译路线；
   排序看 **`re/func_ranking.csv`**（`tools/func_ranking.py`，§7）：用量高 + 依赖少的先做。
2. **ida MCP 测绘**：确认边界、ABI（寄存器参数还是栈参）、全局变量 xref、是否有 int/自修改。
3. **写 C**：`src/game/<模块>.c` + 头部注释写清原地址、语义、打通与否的项目。
4. **写 `*check.c` 对拍**：见 §2 的三种方法，全部以**原始机器码**为参照。
5. **接入 `src/repl.c`**：把原函数入口 5 字节改成 `jmp rel32` 指向 C（默认开，`--replace=` 可分组关）。
6. **宿主 A/B**：同一 autokey + 固定 `--shot-frame`，`--replace=none` vs `all` → `framediff.ps1`，
   差值 ≤ none↔none 基线噪声。
7. **`pwsh -File port\regress.ps1`**：必须 **8/8 PASS**（`-Replace none` 与 `all` 两种都跑）。

## 2. 三种对拍方法（按依赖复杂度递增）

| 方法 | 适用 | 要点 | 出处 |
|---|---|---|---|
| **机器码直调** | 纯计算函数（RLE/gfx/sprite24/util/path/tables/rle2/rec） | LE 加载器映射 FD2.EXE 成 RWX（与 `letest` 同一条可信加载路径）→ 函数指针直接调**原机器码** → 与 C 比对目标缓冲**逐字节** + 全局副作用；随机用例用**合法生成器**（不是纯随机垃圾） | §19.4 |
| **CRT 重定向** | 依赖文件/内存的函数（`res.c`，未来的 `sub_15F84`） | 把 Watcom CRT 的 `fopen/fseek/fread/malloc/free/fclose` 入口头 5 字节改成 `E9 rel32` 跳宿主 libc，再调**原版机器码** —— 不改游戏逻辑、不动 call 位移 | §25.2 |
| **确定性时钟 / 事件序列** | 依赖时间、BIOS、堆/延时的函数（等键、打字机、对话框动画） | 给双方喂同一个**确定性时钟桩**（每读一次 tick 加一）；服务（堆/delay/BDA 冲键/快照）钩成**事件记录桩**，连**调用顺序**都对拍；低内存访问走低内存镜像 | §30.2 §31.2 §33.2 |

> 对拍 exe 都要链 **`/BASE:0x60000000`**：否则它自己的映像会被 ASLR 放进 guest 窗口
> `0x10000..0x6FFFF`，低地址预留直接失败（§22.4）。

## 3. 接入： `src/repl.c`

- 机制：对每个已通过机器码对拍的函数，**入口写 5 字节 `jmp rel32`** 指向 C 实现
  （与 `src/ail.c` 替换 AIL 入口同一机制），在 `le_map_and_relocate` 之后、游戏线程启动前安装。
- **分组即开关单元**：`--replace=none|all|rle,gfx,sprite24,util,path,dlg,rec,svc,vm,res,bgm,scene,fade,map,fx,ev2,ev3,ev4,ev5,ev6,unitload`。
  分组是原子单位——例如 `dlg` 组的开框分配走 CRT `malloc`、收框经 `0x15E71` 归还，必须同开同关。
- **安全性论证**（写在 `src/repl.h`）：被替换的都是普通 cdecl 函数，MSVC 的 callee-saved 寄存器
  是原版保证的超集（唯一例外 `0x4DF09` 原版不保存 EBX，我们的 C 保存，安全）；
  **ida 实证：替换集合之外没有任何代码读这些 scratch 全局**；地址是 FD2 专属，只对 FD2 build 生效。
- **app-level 写法**（第 30 轮起，用于对话框等"业务逻辑"函数）：
  - 全局**留在原地址**用宏访问（`dword_53A18` 等），因为除它之外还有几十上百个未被替换的调用点；
    C 必须读写真正的原字，否则状态分叉。好处是 `repl.c` 里**零包装**（签名与机器码一一对应）。
  - 服务**留在原机器码**（`0x15E9E`/`0x15E71` 快照/还原、`0x3790A` delay、`0x4E381` BDA 冲键）：
    时机耦合 + 快照缓冲跨机器码/C 边界 ⇒ 暂留；它们的缓冲将来也走 `guest_mem` 后再接
    （`res.c` 已按这个模式接入，见 §47 / `rounds/17-res-and-guest-heap.md`）。
  - **跨边界的缓冲一律 `src/game/guest_mem.h`**（`guest_malloc/guest_free`）：宿主里=游戏堆
    （`0x3706E/0x3776E`），check 工具里=被重定向的宿主 libc；阶段 C 只换实现。

## 4. 已完成模块清单

> **机器可读记录**：`re/translation_map.csv`（每个已转译函数一行：原地址 / C 名 /
> 源文件 / 接入分组 / 对拍工具 / 用例数 / 状态）。由 `python tools/translation_map.py`
> 以 `src/repl.c` 为真源生成；`--check` 在不一致时退出 1。当前 **261 wired / 1359（19.2%）**。

对拍用例数与接入状态（接入数合计 **133**）：

| 模块 | 源文件 | 原地址族 | 对拍 exe | 用例数 | 接入分组 | 轮次 |
|---|---|---|---|---|---|---|
| RLE 解码（3 模式 + LUT） | `game/rle.c` | `0x4E98D` `0x4E8D3` | `rlecheck` | 1900 | `rle`（2） | §19 |
| 0xC0-RLE 文本 blit | `game/rle2.c` | `0x4EBFF` `0x4EC31` `0x4EBAB` | `rle2check` | 1200 | `rle`（3） | §28 |
| 图形 blit 工具族 | `game/gfx.c` | `0x4EC7C`…`0x4EEE0` | `gfxcheck` | 1450 | `gfx`（6） | §21 |
| 24×24 精灵 RLE 族（7 变体） | `game/sprite24.c` | `0x4DF84`…`0x4E29C` | `sprite24check` | 2100 | `sprite24`（7） | §22 |
| 字节/调色板工具 | `game/util.c` | `0x4DED4`…`0x4E795` | `utilcheck` | 2200 | `util`（6） | §23 |
| 表访问器（11 个） | `game/tables.c` | `0x4E7DD`…`0x4E8BC` | `tablescheck` | 4528 | `util`（11） | §27 |
| 地形代价洪泛 + 寻路 | `game/path.c` | `0x4E390` `0x4E4F6`（+7 内部） | `pathcheck` | 1000 | `path`（2） | §24 |
| 角色记录表 | `game/rec.c` | `0x34894` `0x12C60` | `reccheck` | 28739 | `rec`（2） | §32 |
| 记录 8 槽字段访问器 | `game/rec.c` | `0x1B722` `0x344F2` `0x1BB8C` `0x1B8E7` | `reccheck` | 32010 | `rec`（4） | §58 |
| 持久队伍记录表 | `game/unit.c` | `0x1145A` `0x11506` `0x112A5` | `reccheck` | 36327 | `rec`（3） | §59 |
| 队伍身份查询 | `game/unit.c` | `0x33499` | `reccheck` | 37398 | `rec`（4） | §61 |
| 记录表七个数据叶子 | `game/rec.c` | `0x1B8A6` `0x1B83D` `0x1CA89` `0x13512` `0x32975` `0x34D64` `0x35009` | `reccheck` | 42225 | `rec`（11） | §64 |
| 对话框辅助 | `game/dlg.c` | `0x16559` `0x16E24` | `dlgcheck` | 800 | `dlg`（2） | §29 |
| 开框/收框动画 | `game/dlg.c` | `0x165AC` `0x16B43` `0x168B6` `0x1685C` | `boxcheck` | 240 | `dlg`（4） | §30 |
| 等键 + 嘴型动画 | `game/dlg.c` | `0x16C57` | `keycheck` | 100 | `dlg`（1） | §31 |
| 打字机步进 | `game/dlg.c` | `0x164E8` | `typecheck` | 1176 | `dlg`（1） | §33 |
| 人像快照存/还原 | `game/dlg.c` | `0x15E9E` `0x15E71` | `boxcheck` | 240 | `dlg`（2） | §48 |
| 换曲入口 `play_bgm` | `game/bgm.c` | `0x25977` | `bgmcheck` | 6000 | `bgm`（1） | §49 |
| 状态机场景卡 `scene_card` | `game/scene.c` | `0x22E5C` | `scenecheck` | 100 | `scene`（1） | §50 |
| 主状态机转移 handler（5 表项） | `game/scene.c` | `0x22EF6` `0x231BC` `0x23790` `0x2389F` `0x23E39` | `scenecheck` | 1100 | `scene`（5） | §60 |
| 主状态机转移 handler（第 6 表项） | `game/scene.c` | `0x239BD` | `scenecheck` | 1480 | `scene`（6） | §61 |
| 调色板淡变三件套 | `game/fade.c` | `0x11D40` `0x1F882` `0x1F525` | `fadecheck` | 4000 | `fade`（3） | §51 |
| 热叶子六件套 | `kbd.c` `util.c` `gfx.c` `res.c` `dlg.c` | `0x4E381` `0x4EBE3` `0x10620` `0x11EB0` `0x2EB9F` `0x12D7B` | `leafcheck` | 72008 | `svc`/`util`/`gfx`/`res`/`dlg` | §52 |
| 地图视图叶子四件套 | `map.c` `fade.c` `res.c` `dlg.c` | `0x126F7` `0x11DF2` `0x16886` `0x134E4` | `mapcheck` | 25000 | `map`/`fade`/`res`/`dlg` | §53 |
| 动画/地图格/头像查找四件套 | `anim.c` `map.c` `dlg.c` `tables.c` | `0x1297D` `0x12E38` `0x12C0D` `0x4EB48` | `mapcheck` | 55500 | `map`/`dlg`/`util` | §54 |
| 数字渲染链 | `dlg.c` | `0x187D6` `0x1875D` `0x1AEB1` | `mapcheck` | 75500 | `dlg`（3） | §55 |
| 格子对象精灵链 | `map.c` `rec.c` | `0x1F183` `0x12AC6` `0x129EC` | `mapcheck` | 86500 | `rec`/`map`（2） | §56 |
| 头像精灵链 | `dlg.c` | `0x127E0` `0x127A9` | `mapcheck` | 97000 | `dlg`（2） | §57 |
| tick 等待 + PCM 音效 | `game/svc.c` | `0x17AA9` `0x25A96` `0x25B45` | `typecheck` | 1616 | `svc`（3） | §33 §37 |
| **脚本/文本 VM（词流解释器）** | `game/vm.c` | `0x15F84` | `vmcheck` | 5512 | `vm`（1） | §39 |
| **资源加载器** | `game/res.c` | `0x111BA` | `rescheck` | 160 | `res`（1） | §25/§47 |
| **效果动画 handler 四件套** | `game/fx.c` | `0x2C217` `0x2CAFC` `0x2CCF4` `0x2CE1A` | `fxcheck` | 3920 | `fx`（4） | §62 |
| **效果动画 handler 收尾（5 handler + 辅助）** | `game/fx.c` | `0x2B996` `0x2BB33` `0x2BD6C`+`0x2BF83` `0x2BFD9` `0x2C441` | `fxcheck` | 12884 | `fx`（5） | §63 |
| **对话框/头像合成三件套** | `game/msg.c` | `0x1956B` `0x1974C` `0x26996` | `msgcheck` | 465 | `dlg`（3） | §65 |
| **`funcs_1199C` 事件 handler 闭包子集** | `game/ev.c` | `0x34738`…`0x35258`（11） | `evcheck` | 2940 | `rec`（11） | §66 |
| **地图视图渲染核** | `game/map.c` | `0x11EEE` `0x24D22` `0x122DC` `0x1ACF3` | `mapcheck` | 112000 | `map`（4） | §67 |
| **调色板动画 + 地图视图刷新** | `game/fade.c` `game/map.c` | `0x4E310` `0x4E31C` `0x32230` `0x11CAC` | `mapcheck` | 122500 | `fade`（2）`map`（2） | §68 |
| **`funcs_1199C` 第二批（30 个）** | `game/ev2.c` | `0x135DD` + `0x35298`…`0x3644E`（29） | `ev2check` | 6000 | `ev2`（30） | §69 |
| **`funcs_1199C` 场景脚本簇收口（28 个）** | `game/ev3.c` | `0x352CA`…`0x362E8`（23）+ `0x35B78`/`0x35F10`/`0x361B0`/`0x2AEDB`/`0x33F78` | `ev2check` | 6000 | `ev3`（28） | §70 |
| **场景移动/地图窗口/头像关闭（10 个）** | `game/ev4.c` | `0x1366A` `0x11AA8` `0x11B48` `0x11B9B` `0x11BFA` `0x11C59` `0x12263` `0x1E1DC` `0x24B4D` `0x196CB` | `ev2check` | 6000 | `ev4`（10） | §71 |
| **小叶子大批量（28 个）** | `game/ev5.c` | `0x2860A` `0x146A7` `0x13460` `0x13536` `0x1D4CB` `0x173E7` `0x24B14` `0x25052` `0x25089` `0x34317` `0x1F6EF` `0x1C220` `0x1E5C0` `0x2B749` `0x26C9B` `0x314DE` `0x1B5F1` `0x14B16` `0x203BD` `0x208CF` `0x20AAF` `0x20BF5` `0x20B72` `0x205B4` `0x205BE` `0x1F04A` `0x1F0DC` `0x1B653` | `ev2check` | 2400 | `ev5`（28） | §72 |
| **单位精灵构建链（7 个）** | `game/unit_load.c` | `0x10B4E` `0x10C50` `0x11019` `0x145CD` `0x14625` `0x1B750` `0x32999` | `ev6check` | 400 | `unitload`（7） | §73 |
| **`funcs_1199C` 低索引 handler（25 个）** | `game/ev6.c` | `0x34531` `0x3460B` `0x34673` `0x346CD` `0x34778` `0x350BE` `0x350C8` `0x34818` `0x348BB` `0x34940` `0x34984` `0x349EC` `0x34A1E` `0x34B07` `0x34B6F` `0x34B9A` `0x34C52` `0x34C7A` `0x34D2F` `0x34DD0` `0x34EB3` `0x34F38` `0x34FC2` `0x34FCC` `0x35022` | `ev2check` | 5000 | `ev6`（25） | §73 |

`obj0` 工具库 `0x4DED4..0x4EEE0` **已全部转译完毕**。

## 5. 下一步

> **进度基线**：`re/funcmap.csv` 共 **1359** 个函数，已接入 **261 个（≈19.2%）**；
> **其余 ~81% 仍是原始机器码**，而且已转译的 C 会回调机器码的固定 32 位地址
> （`game/svc.c` 的 `ORIG_*`）⇒ 宿主必须 32 位，全部源码化后才不需要 `-m32`
> （`rounds/16-entry-layer.md` §46.8）。

1. ~~`sub_15F84` 脚本 VM~~ **已完成（§39）**：`game/vm.c` + `vmcheck` **5512 例全过**，接入分组 `vm`。
   遗留 `0x15E9E`/`0x15E71`（快照/还原）按第 2 条与 CRT 堆整体替换一起接。
2. ~~`res.c` 接入 + `0x15E71`/`0x15E9E`~~ **已完成（§47/§48）**：新增 **`src/game/guest_mem.h/.c`**（唯一堆缝），
   `res.c` 改用 `guest_malloc/guest_free` 并把尺寸写进游戏全局 `0x53BFF`；`0x15E9E`/`0x15E71`
   转成 `dlg_snap_save`/`dlg_snap_restore`（`game/dlg.c`），`dlg.c` 不再留机器码回调。
   接入分组 `res` + `dlg`（**54** 个）。判据：`rescheck` **160/0**、`boxcheck` **240/0**、
   `regress` **8/8**、`none↔all` 同 tick **0 px**、Linux `host32` 同 tick **0 px**。

   当时的调研结论（保留备查）：
   - 游戏自己的堆入口是 `0x3706E malloc` / `0x3776E free`（`re/funcmap.csv` 的 `crt_sym`），
     底下是 `_nmalloc/__MemAllocator/sbrk`。**不整体换成 libc**：CRT 内部（stdio 的
     `_ioalloc` 等）直接用自家堆，只换 `malloc/free` 会变成两堆混用。
   - **先例已是“共用游戏堆”**：`game/dlg.c` 用 `ORIG_ALLOC = 0x3706E` 分配，
     释放走原 `0x15E71`（内部 Watcom `free`）——这就是现有做法。
   - **`res.c` 要改两处才能接**：① `malloc/free` → 游戏堆（`old_buffer` 是游戏给的、
     `buf` 要还给游戏释放）；② `res_size` 现在写的是 C 自己的变量，必须改成写
     游戏全局 `dword_53BFF`（`0x53BFF`），否则游戏读不到；`rescheck` 相应改成
     “每次调用后先快照 `GUEST_SIZE`”。文件 I/O 可继续用宿主 libc（只是读字节）。
   - **落地方式**：抽 `src/game/guest_mem.h`（`guest_malloc/guest_free`）作为**唯一堆缝**
     （宿主里指向 `0x3706E/0x3776E`；check 工具里已被 CRT 重定向成 host malloc），
     供 `res.c`、`dlg.c`（逐步迁移）、将来的 `0x15E71`/`0x15E9E` 共用；
     **阶段 C（去 guest 化）只需把它的实现换成宿主 malloc**。
   - 判据：`rescheck` 仍过（含新的 guest-global 检查）+ `regress` 8/8 +
     同 tick A/B（`--replace=none` vs `all`）**0 px**。
3. **主状态机**（`re/RE_MAP.md`：`0x25EBB`/`0x117E7`/`0x22E5C`/`0x26152`，含
   `funcs_25E23[]`/`funcs_25E3A[]` 函数指针表）：占比最大的一族，一条一条转、
   每条都挂 `*check` 对拍后再接入。**第一刀 `play_bgm`（`0x25977`）已完成（§49）**——
   IDA 侦察澄清了它不属于状态机本体；**`0x22E5C` scene_card 也已完成（§50）**。
   分派表地址已钉死（`funcs_25E23 @0x51DE9`、`funcs_25E3A @0x51D71`，`main`=`0x25BF4`），
   **`0x22EF6` 起的状态 handler 族已完成 6 个（§60–§61）**：
   `scene_state_00/03/10/12/18/14`（`funcs_25E23[0]/[3]/[10]/[12]/[18]/[14]`，
   `game/scene.c`，`scenecheck` 1480/0，接入 **95**）；表内其余 19 项闭包较大
   （> 3 KB，含 `0x122DC` 1051 B 等），需先闭各自闭包。
   与此同时按 `re/func_ranking.csv` 的用量/依赖拓扑收了若干叶子簇（§51–§61）：
   淡变三件套、热叶子六件套、地图视图四件套、动画/地图格/头像查找四件套、数字渲染链、
   格子对象精灵链、头像精灵链 `0x127E0`/`0x127A9`（§57）、
   角色记录 8 槽字段访问器簇 `0x1B722`/`0x344F2`/`0x1BB8C`/`0x1B8E7`（§58，**85 个**）、
   持久队伍记录表三件套 `0x1145A`/`0x11506`/`0x112A5`（§59，**88 个**）——转完
   `funcs_25E23` 分派表 5 个表项的依赖闭包已闭合；
   主状态机转移 handler 五件套 `0x22EF6`/`0x231BC`/`0x23790`/`0x2389F`/`0x23E39`
   （§60，**93 个**）；第 6 表项 `0x239BD` + 队伍身份查询叶子 `0x33499`
   （§61，**95 个**）。
   **第 7 个表项闭包之外另发现一张独立分派表 `funcs_30469`（效果动画）**：
   表项 `[4]/[7]/[8]/[9]` = `fx_dots6`/`fx_dots3`/`fx_dots16`/`fx_toggle` 已完成
   （§62，**99 个**，新模块 `game/fx.c`）；
   剩余 5 个 handler `0x2B996`/`0x2BB33`/`0x2BD6C`/`0x2BFD9`/`0x2C441` + 86 B 辅助 `0x2BF83`
   （即 `fx_dots7/dots8/blob/dots12/dots6b` + `fx_advance`）也已完成
   （§63，**104 个**，`fxcheck` 12884/0）；**整表 9/10 完成**，仅余 `[6] 0x2C67D`
   （1151 B，含 CRT `cos/sin`，另开一轮），转完可让分派器 `sub_31266`（632 B）
   只差 `0x2FB2C`(744)+`0x2FE14`(237) 即依赖闭合。
   **角色记录表七个数据叶子已完成（§64，111 个）**：`0x1B8A6`/`0x1B83D`/`0x1CA89`/
   `0x13512`/`0x32975`/`0x34D64`/`0x35009`（`game/rec.c`，`reccheck` 42225/0）——
   第 28 轮 §58.5 排队的候选至此收完；`0x205BE` 排除（真入口是 `0x205B4`，需另评估）。
   **下一批主候选：`funcs_1199C`(`0x51B91`)**——本轮只收了 `[28]`/`[36]`，表里还有
   `[0]=0x34531`/`[1]=0x3460B`/`[2]=0x34673`/… 一大批同族 80 字节记录服务，
   逐个闭依赖后按表项顺序推进（与 `funcs_25E23` §60–§61 同一打法）。
   **`funcs_1199C` 的依赖闭合子集已完成（§66，接入 125）**：11 个事件 handler → `game/ev.c`
   （`evcheck` 2940/0）；表里剩余 34 项全挂在同一个约 13 KB 的“场景渲染核”上。
   **“场景渲染核”里依赖已闭合的 4 个已完成（§67，接入 129）**：`map_render_view`(0x11EEE)/
   `map_scroll_lines`(0x24D22)/`map_reveal_cursor`(0x122DC)/`map_draw_cursor`(0x1ACF3)
   → `game/map.c`（`mapcheck` 112000/0）。**转完后 `0x197E5` 依赖闭合，
   `0x135DD`(usage 98)/`0x1366A`(110) 只剩 `0x11CAC`(84)**。
   **`0x4E31C` 调色板动画链已完成（§68，接入 133）**：`0x4E310`(`pal_tick_word`)/`0x4E31C`
   (`pal_anim_step`，内联 `out 0x3C8/0x3C9`，check 侧用窄 VEH `out` 陷阱接住)/`0x32230`
   (`map_unit_ping`)/`0x11CAC`(`map_view_update`) → `game/fade.c`/`game/map.c`
   （`mapcheck` 122500/0）。**转完后 `0x135DD`/`0x1366A`/`0x196CB`/`0x11AA8`/`0x11B48/9B/BFA/C59`
   依赖全闭**；下一批首选项即全表 usage 前二 **`0x135DD`(98) / `0x1366A`(110)**（各自成块、单独对拍）。
   **批量节奏第 1/2 批已完成（§69/§70，接入 191）**：`funcs_1199C` 的**场景脚本簇
   索引 38..90 全部源码化**——第 39 轮 30 个（`game/ev2.c`，`REPL_EV2`）+ 第 40 轮
   23 个 + 5 个 helper（`game/ev3.c`，`REPL_EV3`）；对拍 harness `ev2check` 支持 `--only` 子集
   （每 3-5 个验一次）、快照覆盖 obj1+obj2。
   **第 3 批（§71，接入 201）**：全表 usage #1 **`0x1366A`**(110) + 地图窗口 stepper
   `0x11B48/9B/BFA/C59` + 等键 `0x11AA8` + 格子计数 `0x12263` + 光标入队 `0x1E1DC` +
   滚动动画 `0x24B4D` + 头像关闭 `0x196CB` → `game/ev4.c`（`REPL_EV4`）；`ev2check` 扩
   VGA 快照 + 低内存镜像 + INT16 缝。
   **第 4 批（§72，接入 229）**：按操作者“不限制 30、越多越好”，一次收 ready 集里
   28 个小叶子（地图格/记录谓词/调色板/tick/队伍重置/四组 rec_flag 事件）→ `game/ev5.c`
   （`REPL_EV5`）；harness 用记录缓冲当 scratch + `outp` 钩子。
   **第 5 批（§73，接入 261）**：把当时排队的首选项一次做完——**`0x10B4E` 链**
   （`0x10B4E`+`0x10C50`+`0x11019`+`0x145CD`+`0x14625`+`0x1B750`）+ `0x32999` → `game/unit_load.c`
   （`REPL_UNITLD`）；`funcs_1199C` 索引 0..37 剩余 **25 个 handler** → `game/ev6.c`（`REPL_EV6`），
   **整张表 0..90 全部源码化**。新对拍器 `ev6check`（Watcom CRT 七入口重定向 + 合成
   FDICON/FDFIELD；A 组整链、B 组 `0x1B750`）。**下一步首选项（按 usage）**：
   **`0x205DA`(28)**、**`0x197E5`(17)/`0x19953`(17)/`0x14818`(17)**、`0x2FACD`/`0x15F0E`/`0x12CEA`
   （各 15）、`0x233C6`/`0x1E0DB`/`0x1C4CC`（各 15）、`0x2C67D`（`funcs_30469[6]`，含 CRT `cos/sin`）。
   解释器簇 `0x1AA1D`（726）的 105 个被调者现已全部在表（本轮的 25 个 + 之前各批），
   只差 `0x1B932`/`0x22AF6` 等零头即可整体接入。
4. 依赖 BIOS tick / 服务 / 低内存镜像的函数用第 1 条的“确定性时钟 / 事件序列”对拍法
   （`svc.c` 已开先例）。
5. 平台侧（Linux）不再往前推：Wayland 原生明确不做；`make host32`（-m32，跑真游戏）只等
   用户装 i386 工具链（`rounds/16-entry-layer.md` §46.7 有命令）。

## 6. 终点：不依赖 32 位、可上 macOS 的纯 C 引擎

用户目标（2026-10-07 明确）：**全部源码化后不要再跑 32 位兼容模式** —— 32 位会被淘汰，
macOS 自 Catalina 起已完全不支持 32 位。这本身也是路线 C 的定义。

**为什么现在还要 32 位**（`rounds/16-entry-layer.md` §46.8 有实测证据）：
`re/funcmap.csv` 的 1359 个函数只转了 261 个（≈19.2%），其余仍是 `FD2.EXE` 的 32 位机器码；
**并且已转译的 C 会回调机器码的固定 32 位地址**（`game/svc.c` 的 `ORIG_*=(uintptr_t)0x4E310`），
游戏数据段也按 32 位扁平地址写死。所以整机必须 32 位。

**四个阶段**：

| 阶段 | 做什么 | 判据 | 位数 |
|---|---|---|---|
| A 转译（进行中） | 一个函数一个函数还原成 C，`*check` 与**原机器码**逐字节对拍后接 `repl.c` | 对拍用例 + `regress` 8/8 + 同 tick A/B 0 px | 32 位 |
| B 转译完成 | 1379/1359，机器码只作为对照存在 | 无 `--replace` 分组也能跑 | 32 位 |
| C 去 guest 化 | ① `*(int32_t*)(uintptr_t)0x53A2C` 这类**绝对地址访问**改成真正的全局/结构体；② guest 指针改用 `uintptr_t` 语义之外的真指针；③ 删 `le.c`/`dos.c`/`ail.c`（LE 加载、DOS 服务、AIL 替换）换成引擎层；④ `repl.c` 与 `ORIG_*` 一起消失 | 编译成 **64 位**能过全流程 | 开始不需要 |
| D 平台目标 | Windows D3D11 / Linux GL / **macOS Metal**；输入/音频走 sokol（`sokol_audio` 在 mac 是 CoreAudio）；**Metal 需要 MSL shader**（现只有 HLSL+GLSL，需补一份或用 sokol-shdc） | 三平台同 tick 抓帧一致 | 64 位原生 |

**过渡期的纪律（让阶段 C 的改动量可控）**：
- 新转译代码尽量**不把绝对地址写死在逻辑里**，集中到少数访问器（`tables.c` 已开先例）；
- 能走参数/结构体的就别走 `0x53xxx` 全局（原版是 C，多数本来是结构体成员）；
- 明确标注"为对拍而暂时保留的机器码回调"（`ORIG_*`），转译完一个就删一个。

## 7. 排期依据：按"用量"排序（`re/func_ranking.csv`）

`re/funcmap.csv` 每行已带两个 IDA 静态计数，`tools/func_ranking.py` 把它们合成**热度**并
与 `src/repl.c` 的已接入集合 join：

```
usage = callers_game + data_xrefs
        ^ 游戏内直接调用点   ^ 地址被当数据引用（分派表 funcs_25E23[]/funcs_25E3A[]、回调）
```

- 产物 **`re/func_ranking.csv`**（addr/size/name/zone/usage/callers_game/callers_lib/data_xrefs/str_refs/wired）。
- `python tools/func_ranking.py --top 30` 直接打印"还没转的 top N"。
- **caveat**：这是**静态**计数（地点数），不是运行期执行次数；但它是最便宜的可靠排序，
  而且对**只能经分派表到达**的状态 handler（直接调用点 0）是**唯一**的静态信号（看 `data_xrefs`）。

**当前 top 15（未接入，2026-10-08）**：

| addr | size | usage | 是什么 | 备注 |
|---|---|---|---|---|
| `0x3702F` | 16 | 538 | **Watcom 栈溢出探针**（`xchg eax,[esp+arg_0]; call 0x37042; retn 4`） | 编译器运行时，不是游戏逻辑；替换需 `ret 4` + 溢出语义，单独排期 |
| `0x205DA` | 163 | 28 | 事件/交互（调 `0x1088D`/`0x11CAC`/`0x17E0B`/`0x1F525`） | 依赖较多 |
| `0x197E5` | 366 | 17 | 场景绘制（`0x11EB0`/`0x11EEE`/`0x127A9`/`0x1297D`/`0x17D6F`/`0x4ED34`） | 解释器簇成员，依赖几乎全闭 |
| `0x373CA` | 466 | 17 | **Watcom CRT `fread`** | 编译器运行时，不是游戏逻辑；不转 |
| `0x14818` | 480 | 17 | 路径/单位（`0x22AF6`/`0x4E390`/`0x4E8A5`） | |
| `0x19953` | 1188 | 17 | 场景绘制（`0x10620`/`0x11EB0`/`0x11EEE`/`0x127A9`/`0x1297D`/`0x13E9C`/`0x4E31C`/`0x4EBE3`/`0x4EBFF`/`0x4EC31`） | 解释器簇成员 |
| `0x2FACD` | 95 | 15 | `0x18C6D` 包装 | |
| `0x15F0E` | 118 | 15 | 文本/图像（`0x15880`/`0x4EBAB`/`0x4ECBF`） | |
| `0x12CEA` | 145 | 15 | 人像滑入（调地图 stepper `0x11B48/9B/BFA/C59`/`0x11CAC`） | 依赖已闭 |
| `0x233C6` | 245 | 15 | 淡变/地图（`0x11CAC`/`0x13536`/`0x1F525`/`0x1F882`/`0x22AF6`） | 依赖已闭 |
| `0x1E0DB` | 257 | 15 | `0x1088D` + `0x37B29`/`0x37B55` | |
| `0x1C4CC` | 658 | 15 | 场景（`0x11CAC`/`0x11EB0`/`0x17AA9`/`0x25A96`） | |
| `0x2C67D` | 1151 | 15 | `funcs_30469[6]` 效果动画 | 含 CRT `cos/sin`，单独排期 |
| `0x311E5` | 129 | 13 | 待确认 | |
| `0x31BDF` | 106 | 12 | 待确认 | |

**排序策略（hot + 低依赖优先）**：先做“用量高 **且** 依赖少/尺寸小”的叶子——它们立刻减少
机器码执行量，又不把未转译的机器码拖进来；依赖多的热函数（`0x11CAC`/`0x135DD`）等它们的
依赖转掉再做；编译器运行时（`0x3702F`）与 CRT（`callers_lib` 高，如 `0x373CA fread`）另算一层。
按此，下一批建议：`0x197E5` → `0x19953` → `0x14818` → `0x12CEA` → `0x233C6`（解释器/场景簇收口）。
