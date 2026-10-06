# 轮次明细：对话框与 UI
§28 0xC0-RLE 文本 blit、§29 对话框辅助、§30 开框/收框动画、§31 等键 + 动画。
---

## 28. 第 28 轮：0xC0-RLE blit 族转译（0x4EBFF / 0x4EC31 / 0x4EBAB）（2026-10-05）

### 28.1 第二套 RLE：token 与三个变体

obj0 在画面 RLE（0x4E98D）之外还有一套 **0xC0 界 RLE**，由 `sub_15F84` 文本渲染器用来贴
FDTXT/DATO 文本行。共享解码器 `0x4EC66`（usercall：AH=剩余计数、AL=当前值、ESI=流指针）：

```
字节 c <= 0xC0        字面量：像素 = c
字节 c 在 0xC1..0xFF  游程：下一个字节是像素，重复 (c - 0xC0) 次（1..63）
```

三个 blit 都是 `[u16 w][u16 h][流]` → strided 面（行内连续、行间 `stride`、流跨行连续）：

| 转译名 | 原地址 | 行为 |
|---|---|---|
| `rle2_blit` | `0x4EBFF` | 行内从左到右写 |
| `rle2_blit_mirror` | `0x4EC31` | 行内**从右到左**写（镜像；第 0 行会写到 `dst` **之前** w-1 字节） |
| `rle2_blit_trans` | `0x4EBAB` | 行内从左到右，0 字节透明（保留目标像素） |

### 28.2 对拍（`src/game/rle2.c` + `src/rle2check.c`）

每模式 400 例：随机合法流（精确解出 w*h 像素、字面量/游程全覆盖）、随机 stride。

**测试坑**：`rle2_blit_mirror` 第 0 行会写到 `dst` 之前，首版测试把两个 `malloc` 缓冲紧挨着，
一个的"越前"写入踩坏另一个的尾部 → 假失败（44 字节、出现在缓冲区末尾）。修法：两个
目标缓冲各留 **128 字节左余量**并把左余量也纳入比较。修好后：

```
build\rle2check.exe  →  PASS: 1200 cases, 0 failures
```

### 28.3 接入 + 一个回归脚本修正

3 个 blit 加入 `src/repl.c`（`REPL_RLE` 组）⇒ 接入数 **34 → 37**。

顺带修：`regress.ps1` 的环境重试判据原本只匹配 `host.err` 里的 `guest window blocks`，
但同源的另一种表现是 `cannot reserve object region @0x10000`（§8-48 签名 B 的变体），
于是那一次偶发启动失败被当成真失败。判据扩为匹配两者：

```powershell
$envBad = ... -match "guest window blocks|cannot reserve object region"
```

### 28.4 实测判据

```
regress.ps1 (-Replace none / all): ALL PASS 8/8；host.log: repl: installed 37
rlecheck 1900/0  gfxcheck 1450/0  sprite24check 2100/0  utilcheck 2200/0
pathcheck 1000/0  rescheck 160/0  tablescheck 4528/0  rle2check 1200/0
```

### 28.5 下轮入口

1. 继续 `sub_15F84` 的词流解释器：本轮已补齐它依赖的 0xC0-RLE 文本 blit；仍需
   `sub_16559/16C57/16B43/16E24/164E8/12C60/165AC/10620` 等 callee 与 `0x53xxx` 全局区。
   `sub_10620` 已确认只是"BIOS 键盘有待按键"检查（`BDA 0x41A != 0x41C`），`0x4ED7A` 的返回值
   被它忽略（前者不受我们的 void 化影响）。
2. 同类纯函数：`0x4EB59`（按 a1 纵向/横向展开）、`0x4EBE3`（滚动随机）、`0x4EB48`（dword 表）。

---

## 29. 第 29 轮：对话框辅助函数转译（0x16559 / 0x16E24）（2026-10-05）

### 29.1 转译：`src/game/dlg.c`

向着 ★★★ `sub_15F84` 走，先清掉它两个**只依赖 VGA + 两个全局**的辅助函数：

| 转译名 | 原地址 | 语义 |
|---|---|---|
| `dlg_blit_dato` | `0x16559` | 取 DATO 资源偏移表第 `idx` 项指向的子图（`[u16 w][u16 h][rle2 流]`），贴到 `0xA0000+box_pos`；`box_pos==0x9017`（下框）用 `rle2_blit_mirror` 镜像，否则 `rle2_blit` |
| `dlg_scroll_text` | `0x16E24` | 上/下框（`0x728`/`0x9017`）文本区上滚：5 轮各把 `row[j+3]→row[j]`（208 B），再整体 `row[j+4]→row[j]`，末 3 行 `memset(0x4A)` |

两个函数都读原全局 `dword_53C67`（框位置）；`dlg_blit_dato` 另读 `dword_53A85`（DATO 缓冲）。
VGA 在 `0xA0000`，确认已被 `le_reserve_address_space()` 的 64 KiB 块预留/提交，测试可直接读写。

### 29.2 对拍（`src/dlgcheck.c`）

构造合成 DATO 资源（4 个子图，每子图随机 rle2 流）；随机框位置（`0x728`/`0x9017`/
一个无效值测 no-op 分支）；VGA 先填哨兵、跑原版存结果、重置 VGA、跑转译版，**整帧 64000 字节对比**。

```
build\dlgcheck.exe  →  PASS: 800 cases, 0 failures
```

### 29.3 接入 + 新分组

新增 `REPL_DLG` 分组（`--replace=...` 支持 `dlg`），2 个函数加入 `src/repl.c`（包装器从
`0x53A85`/`0x53C67` 读全局后调 C 版）⇒ 接入数 **37 → 39**，mask `0x3F`。

### 29.4 实测判据

```
regress.ps1 ALL PASS 8/8（repl: installed 39, mask 0x3F）
rlecheck 1900/0  gfxcheck 1450/0  sprite24check 2100/0  utilcheck 2200/0
pathcheck 1000/0  rescheck 160/0  tablescheck 4528/0  rle2check 1200/0  dlgcheck 800/0
```

### 29.5 下轮入口

1. 继续 `sub_15F84` 的依赖：`sub_165AC`（开框 5 阶段，688 B，`malloc` 5×26668 + `sub_15E71/
   15E9E` 快照 + `sub_168B6` 格网 + `delay`）、`sub_16B43`（收框，276 B）、`sub_16C57`（等键+
   嘴型，461 B）、`sub_164E8`/`sub_16E24`（已转译）等。这些需要 VGA + `delay` + `malloc`，
   可用同法对拍（VGA 已在地址空间；`delay`/`malloc` 走游戏 CRT，纯）。
2. 之后即可整体转译 `sub_15F84`（词流解释器）并对拍 VGA。

---

## 30. 第 30 轮：开框/收框动画转译（0x165AC / 0x16B43 / 0x168B6 / 0x1685C）（2026-10-05）

### 30.1 转译：app-level 写法 —— 全局留在原地址，服务留在原机器码

`sub_15F84` 的两个大依赖（§29.5-1）已清掉，产物仍在 `src/game/dlg.c`：

| 转译名 | 原地址 | 语义 |
|---|---|---|
| `dlg_open_box` | `0x165AC` | 人像滑入（`0x12CEA`）+ 人像精灵从当前尺寸 `24*AB9+4 × 24*ABD+4` 逐帧收到 `(5, rows)`（每帧 `0x15E9E` 存 VGA→贴精灵→`delay(10)`→冲键→`0x15E71` 还原）；`rows==0` 时按框位补默认（`0x728→2`、`0x9017→112`）；然后 `malloc` 5×26668 B 存进 `dword_53A18[5]`（返回该数组地址），按 `(4,2)(8,3)(12,4)(16,5)(19,5)` 五阶段"先存段再贴框"，段间 `delay(10)`，收尾冲键 |
| `dlg_close_box` | `0x16B43` | 逆序还原 5 段（段间 `delay(10)`，第 0 段不延时），`rows!=0` 再把人像精灵从 `(5, rows)` 放大回全尺寸 |
| `dlg_box_stage` | `0x168B6` | 310×86 框的一阶段格网：16 px 瓦片 `cols×lines` + 3 px 边框，角/边/中填/内填共 26 类瓦片按原序贴（顺序决定重叠像素，逐条对齐机器码） |
| `dlg_frame_tile` | `0x1685C` | 从框资源偏移表（`table+6` 起的 dword 表）取第 `idx` 块贴到 `dest` |

**写法决策（与 §19..§29 的"纯函数 + 参数"不同，本轮是第一批 app-level 转译）**：

- **全局用原地址、IDA 名**（`dword_51A83/53A18/53A81/53AB9/53ABD/53C67` 宏定义在 `dlg.c` 顶部）。
  ida xref 实证：`dword_53A18` **只被 `sub_165AC` 读写**（可自持），其余 5 个全局各有几十上百个
  未替换调用点 —— C 必须读写**真正的原字**，否则状态分叉。好处：`repl.c` 里这 4 个条目
  **零包装**（签名与机器码 cdecl 栈参一一对应），也消除了"包装器没被对拍覆盖"的盲区。
- **服务留在原机器码**，`dlg.c` 直接按地址调用（头部注释写明）：
  - `0x15E9E`（存快照+贴精灵）/`0x15E71`（还原+`free`）：**堆耦合**——`0x15E71` 还有
    `sub_10010`/`sub_1A30B`/`sub_1E98C`/`sub_1EB05`/`sub_1F42D` 等**替换集合之外**的调用方，
    它们递进来的块来自 Watcom CRT 堆，换成 libc `free` 会混堆 ⇒ 与 `res.c` 同一理由**暂不接入**
    （`repl.c` 头注释已记）。开框用 `0x3706E`(CRT malloc) 分配、收框经 `0x15E71` 归还，全在一堆。
  - `0x12CEA`（人像滑入）、`0x3790A`（delay）、`0x4E381`（BDA 冲键）：时机/BIOS 耦合，留原码。
  - `0x4ECBF`/`0x4ED0B` 已转译 ⇒ C 里直接调 `gfx_save_rect`/`gfx_blit_block`。
- **调用约定核实**（反汇编实证，`re/dlg_probe.txt` 等）：这 4 个函数全是**cdecl 栈参 + 调用方
  `add esp`**；入口 `push N; call sub_3702F` 只是 Watcom 栈探针（`xchg eax,[esp+4]` 后探测并还原
  EAX，寄存器参数全是幻影）；`sub_12CEA` 开头把两个**栈参**取进 ESI/EDI，寄存器参数只流进
  `sub_11BFA/11C59/11B9B/11B48` —— 这 4 个函数体**一个参数都不用**（只过栈探针），所以从 C 调它
  只需给对 2 个栈参，其余寄存器随意（原版调用点也确实没设）。

### 30.2 对拍（`src/boxcheck.c`）：VGA + 快照 + 事件序列 + 每次 delay 抓帧

服务被钩成**事件记录桩**（在 `le_map_and_relocate` 之后 jmp 覆盖 5 个入口）：

| 钩点 | 桩行为 |
|---|---|
| `0x3706E` / `0x3776E` | libc `malloc/free`（两次跑同一堆），记录 `alloc(size, tag)` / `free(tag)` |
| `0x3790A` delay | 记录事件 + **整帧 VGA 快照**（动画每一步的画面都在比较范围内） |
| `0x4E381` 冲键 | 记录事件（BDA 时序不在本轮范围，两次跑同一桩） |
| `0x12CEA` 人像滑入 | 记录事件（参数 + 调用瞬间的 `dword_51A83`） |

钩桩是双重目的：既让两次跑看到**完全相同**的服务行为，又把每次调用变成
**（类型, 参数, 当时的 flag）**元组 —— 于是对拍覆盖动画的**调用顺序/参数/状态**，不只是结果像素。

每例先跑原机器码、复位世界（同种子重填 VGA + 重建框资源 + 重置 6 个全局）、再跑 C，比较：

- 开框后：整帧 VGA 64000 B、**5 段快照各 26668 B**（含头 w/h/offset）、`dword_51A83`、
  返回值必须是 `0x53A18`、事件序列、**每次 delay 时的整帧**；
- 收框后：整帧 VGA、事件序列（累计）、每次 delay 的整帧、alloc/free 计数相等（无泄漏）。

矩阵：框位 {`0x728`,`0x9017`,其他} × rows {0,2,7,112} × 人像尺寸 {(0,0),(1,1),(3,2),(6,4),(2,5)}
= 60 组开框+收框；另 60 例 `dlg_box_stage`、120 例 `dlg_frame_tile` 直测 —— 直测用 **128 KiB
scratch 面**（`cols<2`/`lines<2` 时原版会算出**负偏移**，真实代码不会这么调，但算术也得对齐，
scratch 带余量吸收，避免越界访问违例）。框资源 18 块随机 `w,h`（底边框 5 块限制 `h≤4`，
与真实美术一致，保证 rows=112 时不写穿 64 KiB VGA 窗口）。

**踩坑（写进本节以免重犯）**：`free` 的 ptr→tag 归属查表最初从头找第一个匹配 ——
libc 重用已释放地址时，两次跑的堆布局不同，同一指针可能命中不同的 tag，产生**假阴性**
（`event 28 orig=free(6) ours=free(5)`）。改成**桩里根本不 free**：块全程存活 ⇒ 一次跑内指针唯一、
归属确定；测试进程多漏 ~20 MB，可接受。

```
build\boxcheck.exe  →  PASS: 240 cases, 0 failures
```

### 30.3 接入 + A/B 实证

`repl.c` 新增 4 条（`REPL_DLG` 分组内，**零包装**：签名一一对应）⇒ 接入数 **39 → 43**，
mask `0x3F`。分组原子性：开框的 `0x3706E` 分配与收框的 `0x15E71` 释放在同组内，`repl_parse`
按组开/关，不存在半开状态（`repl.c` 头注释记了这条约束）。

宿主 A/B（新工具 `framediff.ps1`：两帧逐像素差，数量/百分比/最大通道差）。找到对话框帧的
配方：`--autokey=5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN;3000:RETURN×5` +
`--shot-frame=1500`（画面：上框文本 + 人像 + 下方红框，`build/ab_probe.png`）：

| 对比（同帧 1500） | 差异像素 |
|---|---|
| none vs none | 76 / 64000（0.1187%，帧定时噪声基线） |
| **none vs all** | **0 / 64000（0.0000%）** |
| 同配方帧 1620 none vs all | **0 / 64000** |

⇒ 含对话框的两帧在装/不装 43 个转译时**逐像素相同**，且不超基线；结合 boxcheck 逐字节对拍，
判定接入等价。两帧 PNG：`build/ab_s1500.png`（=none）、`build/ab_a1.png`/`ab_a2.png`（=all）。

### 30.4 实测判据

```
boxcheck 240/0   dlgcheck 800/0   rlecheck 1900/0   gfxcheck 1450/0
sprite24check 2100/0  utilcheck 2200/0  pathcheck 1000/0  rescheck 160/0
tablescheck 4528/0    rle2check 1200/0
regress.ps1 ALL PASS 8/8（repl: installed 43, mask 0x3F）
framediff: frame1500 none↔all 0/64000（基线 none↔none 76/64000）、frame1620 none↔all 0/64000
```

### 30.5 下轮入口

1. **`sub_16C57`（等键 + 嘴型，461 B）** —— `sub_15F84` 剩下的最后一个大依赖。它的服务面比
   本轮宽：读 `BDA 0x46C` 计时（`mov edx,46Ch` 是可被 `dos_patch_lowmem_refs` 重定向的 imm32）、
   `int386(0x16)` 取键（`word_53A8D`）、`sub_10620`（BDA `0x41A/0x41C` 待键判断）、
   `sub_4E31C`（写 `0x3C8/0x3C9` DAC 端口 —— ring 3 会 #GP，对拍需 VEH 服务端口指令）、
   以及已转译的 `sub_1685C`。对拍方案二选一：(a) 给 boxcheck 加 VEH（端口 + 慢速 tick 线程 +
   注键），(b) 全部钩桩并记录事件（本轮手法的延伸，键/时序由桩注入保证两次一致）。
2. 之后整体转译 `sub_15F84`（词流解释器）并对拍 VGA —— 它的其余依赖（`0x16559/0x16E24/0x164E8`、
   rle2、gfx、`0x4ED7A`、`sub_111BA`）均已转译或已有对拍。
3. 排期项（不阻塞）：CRT 堆 + 文件层整体替换后，`res.c`、`0x15E71/0x15E9E` 一起接入。

---

## 31. 第 31 轮：`dlg_wait_key`（0x16C57）转译 + 低内存镜像对拍（2026-10-05）

### 31.1 转译：`src/game/dlg.c` 的 `dlg_wait_key`

`sub_16C57`（461 B，`re/dlg16C57.txt`）是对话框"等键 + 嘴型"例程，被 `sub_15F84` 的 6 个调用点
和 `sub_10010`/`sub_190AC`/`sub_17AED` 共用。语义（逐条从反汇编钉死）：

- 文本区基址 `var_1C = 0x47A0`，`dword_53A51 == 0` 时为 `0x4770`；文本区起点
  `var_20 = 0xA0B4F`（上框，`dword_53C67 == 0x728`）否则 `0xA951F`（下框）。
- `speaker == 1` 时先贴人像瓦片 18（`sub_1685C(area+base+0x640, 320, dword_53A81, 18)`）。
- 等待循环：每轮先 `sub_4E31C`（调色板动画，写 DAC 端口），若 **BDA 0x46C 与上一次读数之差 ≥ 2**：
  - `speaker == 1` 且计数器到 3 → 瓦片在 18/19 之间翻转（19 之后回 18）；
  - 嘴型状态机：已开口 → 贴 DATO 子图 0 并重新掷 `rand % 30 + 2` 作为下一次保持时间；
    未开口 → 保持数递减到 0 时贴 DATO 子图 3 并置"已开口"；
  - 刷新 tick 基准。
- 退出循环（`sub_10620`：BDA `0x41A != 0x41C`，即键盘环形缓冲有键）后，`speaker == 1` 贴瓦片 13（闭嘴）。
- 最后 `word_53A8D` 高字节置 `10h` → `int386(0x16)`（BIOS 读键，不回显），扫描码归一化
  `E0h/52h → 1Ch`、`53h → 01h`，**返回值就是归一化后的扫描码**（`jmp 0x15D9A` 复用 `sub_15DA2` 的
  函数尾，`eax` 直接返回，见 `re/dlg16C57_tail.txt`）。
- **坑**：tick 比较是 16 位的（`mov ax,[46Ch]` 后 `cwde`、`movsx`），C 侧必须用 `int16_t` 再符号扩展，
  否则 tick 回绕时差值符号会翻。

### 31.2 对拍（`src/keycheck.c` + `build.ps1 -Target keycheck`）：低内存镜像 + 确定性时钟

这个函数在普通对拍里跑不了：它的循环条件读 **BDA 0x41A/0x41C**、计时读 **BDA 0x46C**，
都在低 64 KiB（Windows 不可映射），而且"跑两次"本身不确定。所以 harness 做两件事：

1. **低内存镜像 + 操作数重定向**（与宿主 `dos.c` 同一算法）：把 `mov/push reg, imm32` 中
   `imm ∈ [0x400,0x500)` 改写成 `mirror + imm`（FD2 build 实测 **65 处**），于是**原始机器码**
   读的是 `0x70000` 处的镜像 —— 两侧读同一块内存。
2. **两个钩桩把世界变确定并可观测**：
   - `0x4E31C`（调色板步进）→ `tick++`、记事件、抓一帧 VGA，第 N 次调用时让"有键"成立
     （`tail = head + 2`）；
   - `0x370F0`（`int386`）→ 按脚本返回 AH（记 `intno` + 传入的 AH）。

   两侧看到完全相同的 tick 序列、同一个键、同一串服务调用，所以比较覆盖的是**循环结构本身**：
   最终 VGA + 每次调色板步进的整帧快照 + 事件序列 + 最终 tick + `word_53A8D` + `word_627B8`（rand 状态）。
   随机种子按用例挑选，使首次嘴型保持落在 2..4 tick（上限 12..16 步），保证**开口和闭口两条路径都跑到**。

### 31.3 实测判据

```
keycheck: redirected 65 low-memory references to 0x70000
PASS: 100 cases, 0 failures
repl: installed 44 translated function(s) (mask 0x3F)   # 0x16C57 已接入
regress.ps1 ALL PASS 8/8（15.2 s）
host: working directory = E:\FD2
```

### 31.4 `--volume`：音量此前在 synth 后端不生效（补完）

`§12` 收尾清单里的"音量在 synth 后端不生效"已修：新增 `--volume=0..100`（默认 10），
只在 **waveOut 边界**衰减 —— 数字样本衰减的是宿主侧的 PCM 副本（8 位无符号 / 16 位有符号两条路径），
音乐在渲染完成后衰减播放缓冲。`--midi-dump` 的 WAV 和 `synth: rendered ...` 的统计仍按满量程输出，
所以离线音乐核对的证据不变、管线（合成、AIL 音量渐变、waveOut 提交）行为与之前完全一致。
`AIL_set_sample_volume` 此前"记录但不应用"，现在按 `master × (game volume/127)` 应用。

判据：`--volume=50` 跑 12 s → `ail: master output volume = 50% (music + SFX scaled at waveOut)`，
同轮 `synth: rendered 2256 notes ... peak 32258/32767`（满量程统计仍在）。

> **默认值后来改过（2026-10-06）**：当时定 `10` 是为了"能在旁边干活不受打扰"，但这让**自己玩**
> 时也得手敲 `--volume`。现在默认改回 **`100`（= 加 `--volume` 之前的听感，不衰减）**，
> 调试/自动跑时**显式**加 `--volume=10`（`regress.ps1` 已内置）。
> 改动点在 `host.c` `g_volume`、`ail.c` `g_master_volume`、`synth.c` `g_master` 三处。

### 31.5 实测坑：测试机被别人按键干扰 → `--no-user-input`

`regress.ps1` 的"卡在载入菜单"偶发失败根因：测试期间本机有人在用，**焦点在游戏窗口时按下的键
直接进 BDA 环形缓冲**，autokey 的按键计划因此失步（计划注入的键和实际读到的键对不上）。
宿主新增 `--no-user-input`：真实键盘忽略，只有 `--autokey` 通过入口层自己的投递口送键。
`regress.ps1` 已带上该参数；`--autokey` 路径不受影响。

### 31.6 下轮入口

1. `sub_164E8`（113 B，已反编译 `re/dlg_deps.txt`）—— 依赖 `sub_25A96` 与 `sub_17AA9`，
   这两个是文本绘制/光标侧的函数，需要先定边界。
2. 之后整体转译 `sub_15F84`（1380 B 词流解释器）并对拍 VGA：它的依赖里除 `sub_164E8` 外
   （`0x16559/0x16E24/0x16C57`、rle2、gfx、`0x4ED7A`、`sub_111BA`）均已转译或有对拍。
3. 排期项（不阻塞）：CRT 堆 + 文件层整体替换后，`res.c`、`0x15E71/0x15E9E` 一起接入。
