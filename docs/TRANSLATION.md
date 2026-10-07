# 源码转译（路线 C 主体）总览

> 这是 `PROGRESS.md` 第 19–33 轮的**方法与现状提炼**；每一轮的完整病历（为什么不这么写、
> 踩了什么、判据是什么）在 `docs/rounds/*.md`，细节以轮次文件为准。

**目标**：把 `FD2.EXE` 的机器码整体还原成**人类会写的 C**（不像反编译器输出），
最终产出可编译 x86-64 的引擎。前提是**程序必须一直能运行**：每个模块先接入跑起来，
再谈优雅。

> ⚠ **进度基线（别被“已接入 52 个”误导）**：`re/funcmap.csv` 共 **1359** 个函数，
> 现已完成接入的 **65 个 ≈ 4.8%**（清单见 §4），**其余 ~96% 仍是原始机器码在宿主进程里执行**。
> 这也是路线 C 中期必须用 32 位宿主（Linux `-m32`）的原因；**全部转译完就不再需要 `-m32`**。

## 1. 转译流程（每轮都走这七步）

1. **选模块**：叶子优先（工具层/解码器 → UI → VM），依赖越少越好 → `re/RE_MAP.md` 有转译路线；
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
- **分组即开关单元**：`--replace=none|all|rle,gfx,sprite24,util,path,dlg,rec,svc,vm,res`。
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
> 以 `src/repl.c` 为真源生成；`--check` 在不一致时退出 1。当前 **65 wired / 1359（4.8%）**。

对拍用例数与接入状态（接入数合计 **65**）：

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
| 对话框辅助 | `game/dlg.c` | `0x16559` `0x16E24` | `dlgcheck` | 800 | `dlg`（2） | §29 |
| 开框/收框动画 | `game/dlg.c` | `0x165AC` `0x16B43` `0x168B6` `0x1685C` | `boxcheck` | 240 | `dlg`（4） | §30 |
| 等键 + 嘴型动画 | `game/dlg.c` | `0x16C57` | `keycheck` | 100 | `dlg`（1） | §31 |
| 打字机步进 | `game/dlg.c` | `0x164E8` | `typecheck` | 1176 | `dlg`（1） | §33 |
| 人像快照存/还原 | `game/dlg.c` | `0x15E9E` `0x15E71` | `boxcheck` | 240 | `dlg`（2） | §48 |
| 换曲入口 `play_bgm` | `game/bgm.c` | `0x25977` | `bgmcheck` | 6000 | `bgm`（1） | §49 |
| 状态机场景卡 `scene_card` | `game/scene.c` | `0x22E5C` | `scenecheck` | 100 | `scene`（1） | §50 |
| 调色板淡变三件套 | `game/fade.c` | `0x11D40` `0x1F882` `0x1F525` | `fadecheck` | 4000 | `fade`（3） | §51 |
| 热叶子六件套 | `kbd.c` `util.c` `gfx.c` `res.c` `dlg.c` | `0x4E381` `0x4EBE3` `0x10620` `0x11EB0` `0x2EB9F` `0x12D7B` | `leafcheck` | 72008 | `svc`/`util`/`gfx`/`res`/`dlg` | §52 |
| tick 等待 + PCM 音效 | `game/svc.c` | `0x17AA9` `0x25A96` `0x25B45` | `typecheck` | 1616 | `svc`（3） | §33 §37 |
| **脚本/文本 VM（词流解释器）** | `game/vm.c` | `0x15F84` | `vmcheck` | 5512 | `vm`（1） | §39 |
| **资源加载器** | `game/res.c` | `0x111BA` | `rescheck` | 160 | `res`（1） | §25/§47 |

`obj0` 工具库 `0x4DED4..0x4EEE0` **已全部转译完毕**。

## 5. 下一步

> **进度基线**：`re/funcmap.csv` 共 **1359** 个函数，已接入 **54 个（≈4.8%）**；
> **其余 ~96% 仍是原始机器码**，而且已转译的 C 会回调机器码的固定 32 位地址
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
   下一步按表项顺序做 `0x22EF6` 起的状态 handler 族。
4. 依赖 BIOS tick / 服务 / 低内存镜像的函数用第 1 条的“确定性时钟 / 事件序列”对拍法
   （`svc.c` 已开先例）。
5. 平台侧（Linux）不再往前推：Wayland 原生明确不做；`make host32`（-m32，跑真游戏）只等
   用户装 i386 工具链（`rounds/16-entry-layer.md` §46.7 有命令）。

## 6. 终点：不依赖 32 位、可上 macOS 的纯 C 引擎

用户目标（2026-10-07 明确）：**全部源码化后不要再跑 32 位兼容模式** —— 32 位会被淘汰，
macOS 自 Catalina 起已完全不支持 32 位。这本身也是路线 C 的定义。

**为什么现在还要 32 位**（`rounds/16-entry-layer.md` §46.8 有实测证据）：
`re/funcmap.csv` 的 1365 个函数只转了 54 个（≈4.8%），其余仍是 `FD2.EXE` 的 32 位机器码；
**并且已转译的 C 会回调机器码的固定 32 位地址**（`game/svc.c` 的 `ORIG_*=(uintptr_t)0x4E310`），
游戏数据段也按 32 位扁平地址写死。所以整机必须 32 位。

**四个阶段**：

| 阶段 | 做什么 | 判据 | 位数 |
|---|---|---|---|
| A 转译（进行中） | 一个函数一个函数还原成 C，`*check` 与**原机器码**逐字节对拍后接 `repl.c` | 对拍用例 + `regress` 8/8 + 同 tick A/B 0 px | 32 位 |
| B 转译完成 | 1365/1359，机器码只作为对照存在 | 无 `--replace` 分组也能跑 | 32 位 |
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

**当前 top 15（未接入，2026-10-07）**：

| addr | size | usage | 是什么 | 备注 |
|---|---|---|---|---|
| `0x3702F` | 16 | 538 | **Watcom 栈溢出探针**（`xchg eax,[esp+arg_0]; call 0x37042; retn 4`） | 编译器运行时，不是游戏逻辑；替换需 `ret 4` + 溢出语义，单独排期 |
| `0x11EB0` | 62 | 115 | 跨 stride 的整行拷贝（`memmove` 循环） | **叶子**，好接 |
| `0x1366A` | 818 | 110 | 待确认 | 大函数 |
| `0x135DD` | 141 | 98 | 逐格滚动/移动动画（调 `0x11CAC`+`0x4E381`） | 依赖 2 |
| `0x11CAC` | 148 | 84 | 场景/地图重绘（调 `0x1297D/0x11EEE/0x122DC/0x127A9/0x1ACF3`） | 依赖多 |
| `0x2EB9F` | 66 | 82 | LMI 子图 RLE blit（调已转译 `rle_decode`） | **叶子**，好接 |
| `0x4E381` | 15 | 64 | 清键盘缓冲（`MEMORY[0x41C]=MEMORY[0x41A]`） | **叶子** |
| `0x10B4E` | 258 | 58 | 待确认 | |
| `0x1956B` | 352 | 52 | 待确认 | |
| `0x26996` | 119 | 42 | 待确认 | |
| `0x187D6` | 186 | 41 | 待确认 | |
| `0x4EBE3` | 28 | 40 | 随机数（`ROL2` ×3 on `word_627B8`） | **叶子** |
| `0x126F7` | 178 | 37 | 待确认 | |
| `0x11DF2` | 190 | 34 | 待确认 | |
| `0x12D7B` | 49 | 28 | 人像滑入包装（调 `0x12CEA`，dlg 已按 `ORIG_GLIDE` 用过） | **叶子** |

**排序策略（hot + 低依赖优先）**：先做"用量高 **且** 依赖少/尺寸小"的叶子——它们立刻减少
机器码执行量，又不把未转译的机器码拖进来；依赖多的热函数（`0x11CAC`/`0x135DD`）等它们的
依赖转掉再做；编译器运行时（`0x3702F`）与 CRT（`callers_lib` 高）另算一层。
按此，下一批建议：`0x4E381` → `0x4EBE3` → `0x10620`（按键待取）→ `0x11EB0` → `0x2EB9F`
→ `0x12D7B`（约 350 个调用点、6 个叶子函数）。
