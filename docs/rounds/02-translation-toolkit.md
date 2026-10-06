# 轮次明细：叶子工具层转译开工
§19 RLE 解码（首次建立机器码对拍法）、§21 图形 blit 工具族、§22 24×24 精灵 RLE 族、
§23 字节/调色板工具。
---

## 19. 第 19 轮：源码转译开工 —— RLE 模块（0x4E98D/0x4E8D3）+ 机器码对拍（2026-10-05）

**背景**：用户决定**停止 FDPS 支持**（§7.8 已冻结），全部资源回到初始目标——逆向 FD2 为
高级语言源码（§7.4 “逐步源码化”正式开工）。本轮交付：**第一个转译模块 + 可复用的对拍方法**。

### 19.1 模块测绘（ida MCP，产物都在 `re/`）

RLE 解码位于游戏自带工具库（0x4DED4..0x4EF29）内，家族成员：

| 地址 | 大小 | 身份 | 状态 |
|---|---|---|---|
| `0x4E98D` | 443 | **RLE 解码 · 三模式**（流内色 / 平涂 / 8 色 ramp） | ✅ 本轮转译 |
| `0x4E8D3` | 186 | **RLE 解码 · 调色板 LUT 变体**（`color = table[c]`，a6=表指针） | ✅ 本轮转译 |
| `0x4EC7C`/`0x4ECA4` | 40/27 | 无压缩块解码（读 [w][h] 头后逐行 memcpy；**usercall 寄存器传参** edi/esi/ebp） | 下一批 |
| `0x4ECBF`/`0x4ECF0` | 49/27 | 无压缩块打包（上者的逆操作） | 下一批 |
| `0x4EB48` | 17 | `(&off_627D8)[a1]` 函数指针表访问器 | 待归类 |
| `0x4ED7A` | 188 | 16×16 字形渲染（1 位/像素 + 前景/阴影双色 + 可选底色填充） | 下一批 |

全局状态 `word_627B4/word_627B6`（0x627B4/0x627B6）的全部 xref 都落在 0x4E8D3..0x4ECF9
⇒ 是本家族私有的“当前图宽 / 剩余行数”（宿主崩溃转储读它做诊断，§6 第 4 条）。

### 19.2 流格式与语义（从反汇编钉死，`re/sub_4E98D.disasm`）

头：`[u16 width][u16 height]`，随后 token 流连续跨行消费，每行末尾目的地跳过 `pitch - width`
（行回车）。token = `[2-bit 类型 | 6-bit count-1]`：

| token | 行为 | 流内字节 |
|---|---|---|
| `00xxxxxx` | 实心 run：count 个连续像素 | +1 颜色字节 |
| `01xxxxxx` | **隔位 run**：写在奇数位置，消耗 2×count 像素 | +1 颜色字节 |
| `10xxxxxx` | 字面量：count 个原始像素 | +count 字节 |
| `11xxxxxx` | 跳过：count 个透明像素 | 无 |

三种颜色模式（`rle_decode` 的 a6，**流的消费方式在所有模式下完全一致**，只影响写入值）：
`-1` = 流内原色（已验证调用点绝大多数是它）；`(u16)a6 <= 0xFF` = 平涂 `(u8)a6`（剪影）；
其余 = 8 色 ramp `(u8)a6 + (((u16)a6 >> 8) + c) & 7`（**实用调用点未确认，按字节语义保留**）。
`rle_decode_lut`（0x4E8D3）= `table[c]` 查表。副作用：进函数写宽/高全局，结束时剩余行数递减到 0。

调用点实证（`re/rle_args_*.txt`，39 + 4 个站点）：

- 已确认的模式几乎全是 `-1`；典型用法（`sub_2F4D4` 反编译）：
  `sub_4E98D(BG.DAT 资源缓冲, 0, 50, dst, 640, -1)`。
- 4 个看起来神秘的 `push dword_5413F[edx*4]` 站点：`dword_5413F[i]` =
  `sub_111BA("BG.DAT", i)` 返回的**资源缓冲指针**，在调用序列里是 **src（a1）不是模式** ——
  靠 `sub_2F4D4` 反编译解开（静态 push 扫描容易把实参序搞反，教训）。
- `sub_4E8D3` 的 4 个调用点 a6 传缓冲区/表指针（LUT）✓。

### 19.3 转译产物

| 文件 | 内容 |
|---|---|
| `src/game/rle.h` | 格式文档（token 表、模式语义、全局副作用）+ API |
| `src/game/rle.c` | `rle_decode`（原 0x4E98D）+ `rle_decode_lut`（原 0x4E8D3）；两者共用一条 token 状态机（原版是两份手写副本） |
| `src/rlecheck.c` | **机器码对拍测试**（build.ps1 新目标 `rlecheck`） |
| `re/sub_4E98D.c/.disasm`、`re/sub_4E8D3.c`、`re/sub_4EC*.c`、`re/sub_4ED7A.c`、`re/sub_2F4D4.c` 等 | IDA 反编译/反汇编存档 |

### 19.4 对拍方法（可复用于后续每个模块）

`build\rlecheck.exe`：用 LE 加载器把 FD2.EXE 映射成 RWX（与 letest 同一条可信加载路径），
**通过函数指针直接调用原始机器码**，与转译 C 解码同一批随机流，断言：

1. 目标缓冲逐字节一致；
2. 副作用一致（原版读 0x627B4/0x627B6 vs 转译版 `rle_width/rle_height`）。

随机流是**合法**生成器（每行像素精确填满、4 类 token 全覆盖）；覆盖 10 个模式值
（-1、flat 0/0x37/0xFF、ramp 0x100/0x407/0x1234、`(u16)0x37` flat 边界、0xFFFFFF、0xFFFFFFFF）
+ LUT 模式 400 例。

### 19.5 实测判据

```
build\rlecheck.exe  →  PASS: 1900 cases, 0 failures（首轮即通过）
regress.ps1         →  ALL PASS 8/8（build.ps1 新目标未影响宿主）
```

### 19.6 下轮入口

1. 下一批转译：无压缩块族（`0x4EC7C`/`0x4ECBF`，注意 **usercall 寄存器传参**，对拍需要
   `__asm` 寄存器 thunk 设 edi/esi/ebp）、字形渲染 `0x4ED7A`、工具库其余函数
   （`re/funcmap.csv` 中 0x4DED4..0x4EF29 段约 60 个）。
2. ★★★ 级：资源加载器 `sub_111BA`、脚本 VM `sub_15F84`（对拍同法，但要先抽清全局状态区）。
3. 模块归类（RE_MAP §5 阶段 1）继续：`lib_nosym` 剩余、`gfx_A0000` 名单精化。
4. FDPS 相关工作全部冻结（§7.8）。

---

## 21. 第 21 轮：官方逆向知识库（`docs/`）评估 + 图形 blit 工具族转译（2026-10-05）

### 21.1 `port/docs/` 是什么、能不能直接用

`port/docs/` 是 `github.com/wicanr2/fd2_re`（同游戏的 Go/Ebiten **重制**项目）的 docs 快照：
845 个文件 / **28 MB**，`knowledge-base/` 78 份主题文档 + `data/`（IDA 转储、覆盖矩阵、
逐章证据、UI traces、`fd2_function_inventory.json` 等）。**对本项目有用**：函数级语义
（如 `sub_15F84` 文本渲染器、`sub_111BA` 资源加载器）、调用点实参、数据结构与各 U I 资源
的用途，能显著省掉重新摸索的时间。检索用 `rg`，批量结论写文件、不要灌进上下文。

⚠ **重大差异（必须记住）**：docs 分析的 FD2.EXE 是**另一个 build**：

| | docs 的 build | 本项目在跑的 build |
|---|---|---|
| 大小 | 357074 B | **509158 B** |
| md5 | `b97caf2239a27a896069d03549d96e1e` | **`a6e341a8decc6ebf7f4872076d9cf161`** |
| 映像 | `E:\Games\FDCollection\Game\FD2\FD2.EXE` | `E:\FD2\FD2.EXE`（`FD2.EXE.i64` 记录同一 md5） |

- 大函数边界/地址高度重合（`sub_15F84`=0x15F84/1380 B、`sub_111BA`=0x111BA/235 B 在两边
  起止与大小**完全一致**），所以 docs 的高层语义基本可直接参考；
- 但**部分立即数与 call 目标不同**（实证：`0x15FC4` 的比较立即数、`0x165AC` 的 call 位移
  `0x36CD2` vs `0x3702A`），且 `0x4E8xx` 图形族布局不同 —— docs 的 `sub_4EA2A`（字形渲染）
  在本 build 是 `0x4ED7A`；本 build 的 `0x4E98D` 是 443 B 的 RLE，直接覆盖 docs 所谓的
  `0x4E8xx..0x4EAxx` 区（docs 的 `sub_4E8AF`/`sub_4E8E1`/`sub_4E92C`/`sub_4E96F` 在本
  build 对应的是 `0x4E8A5`/`0x4E8BC`/… 的另一批函数）。
- **结论/约定**：docs 只作语义线索；任何**地址 / 常量 / 指令字节**必须以本 build 的
  `E:\FD2\FD2.EXE.i64`（ida MCP）复核。`docs/` 为 28 MB 外部快照，加入 `.gitignore`，不入库。

### 21.2 转译：图形 blit 工具族 `src/game/gfx.c`

§19.6 点名的"下一批"其实是一个自洽的 obj0 工具库（0x4EC7C..0x4EEE0），全部是**纯内存
操作**（目标/来源指针都由参数给出），因此可以像 RLE 一样用"机器码对拍"验证。本 build 的
六个入口（初始 IDA 只把一半识别成独立函数，usercall 半截函数要用别名/手工对齐）：

| 转译名 | 原地址 | 作用 | 原传参 ABI |
|---|---|---|---|
| `gfx_save_rect` | `0x4ECBF`+`0x4ECF0` | strided 曲面 → 紧凑矩形记录 `[u16 w][u16 h][i32 off][pixels]`（**保存**，取源 stride） | cdecl 6 参 |
| `gfx_restore_rect` | `0x4EC7C`+`0x4ECA4` | 记录 → `surface+off`（**恢复**，取目标 stride；off 是记录里的字段） | cdecl 3 参 |
| `gfx_blit_block` | `0x4ED0B` | `[u16 w][u16 h][pixels]` 不透明块 → strided 目标 | cdecl 3 参 |
| `gfx_blit_transparent` | `0x4ED34`+`0x4ED4F` | 同上，**0 字节透明**（不覆盖目标） | cdecl 3 参 |
| `gfx_draw_glyph` | `0x4ED7A` | 16×16 1bpp 字形：每行 u16（**先 xchg al,ah**），置位画前景，另在**下一行同列/左一列**画阴影；`fill!=0` 先平铺整格；**字形 index==10 跳过不画** | cdecl 7 参 |
| `gfx_expand_scanlines` | `0x4EEE0` | 192 行、每行从 `src+4+row*320+table[idx]` 复制 312 B 到 320-B 步进目标，`idx` 每行 +1 模 16 | cdecl 3 参 |

配套：`byte_627C8`（16 B scanline 相位表，**全镜像只有 1 个只读 xref**）已作为常量
`gfx_phase_table[16]` 嵌入；原 0x627A3..0x627B0 的一堆 scratch 全局以 `gfx_pen_*`/`gfx_rec_*`
镜像出来供对拍断言。细节语义与边界（16 位宽度/行计数、`gfx_blit_transparent`/`gfx_draw_glyph`
把 stride **截断成 u16**、`gfx_restore_rect`/`gfx_blit_block`/`gfx_save_rect` 用完整 32 位 stride）
见 `src/game/gfx.h`。

### 21.3 对拍（`src/gfxcheck.c` + `build.ps1 -Target gfxcheck`）

不同 usercall 内部实现（`0x4ECA4`/`0x4ECF0` 走 edi/esi/ebp 寄存器）**不用 `__asm` thunk**：
它们只被 cdecl 包装器调用，而包装器自己装寄存器，所以直接按 cdecl 调 `0x4EC7C`/`0x4ECBF`/
`0x4ED34` 即可。测试用与 `rlecheck` 同一可信 LE 加载路径（映射 FD2.EXE 为可执行、应用 fixup），
对每类随机用例同时跑原机器码与 C 实现，断言：

1. 目标缓冲**含哨兵余量**逐字节一致（能抓到越界/未写的字节）；
2. 原 scratch 全局（0x627B4/B6/A3/A5/A6/A7/AC/B0）与镜像 C 全局一致；
3. 嵌入的 `gfx_phase_table` 与镜像 `byte_627C8` 一致。

覆盖：save↔restore 往返 300、非透明块 300、透明块 300（约半数 0 字节）、字形 400
（含 `fill=0` 与 `index=10`）、scanline 重排 150。

### 21.4 实测判据

```
build\gfxcheck.exe  →  PASS: 1450 cases, 0 failures（首轮即通过）
build\rlecheck.exe  →  PASS: 1900 cases, 0 failures（回归，未受影响）
regress.ps1         →  ALL PASS 8/8（fd2host 未改，15 s）
```

### 21.5 下轮入口

1. `gfx_draw_glyph` 的消费方 —— 文本渲染器 `sub_15F84`（docs 有逐控制码语义可参照，
   但以本 build 复核）；先抽清它的 usercall 14 寄存器参数与全局状态区。
2. 资源加载器 `sub_111BA`（LMI 容器：`fseek(4*index+6)` 读 `{start,end}` → malloc → 读入，
   并释放旧指针）—— 可作下一个"叶子 + 对拍"目标，或直接接进宿主以替换原生 fopen 路径。
3. 图形工具库剩余成员（0x4DED4..0x4E866 绘图/数学原语）继续分类转译。
4. FDPS 相关工作仍冻结（§7.8）。

---

## 22. 第 22 轮：docs 知识库清理 + 24×24 精灵 RLE 族转译（2026-10-05）

### 22.1 `docs/` 清理（28 MB / 845 文件 → 9 MB / 274 文件）

按"对理解 FD2.EXE 是否有用"取舍（清单见 `docs/KEEP.md`）：

| 保留 | 删除（上游重制运营产物，可重新 clone 恢复） |
|---|---|
| `knowledge-base/` 67 篇 + `scene-decode/`（格式、函数语义、逐章 RE 证据） | `SESSION-HANDOFF`、`91-worklist*`、`99-reflections`（历史）、`18/38/41/60/61/96`（重制工程） |
| `data/ida/*.txt`（106 份原始 IDA 证据）+ `fd2_function_inventory.json`、`fd2_unknown_footprints.json` | `data/ida/*.json`（86 份逐章验证收据） |
| `data/exe_tables/*.json`（EXE 抽出的数据表）、`data/*.txt`/`*.md`（原始反汇编） | `data/ui-traces/`、`data/parity-plans/`、`data/parity-slots/`、`data/chapter_beats/` |
| `data/*.json` 21 份跨切面游戏数据（战斗事件/肖像/武器/商店/字形…） | `localization`/`video`/`schema`/`verification`、remake worklist/进度 JSON |

### 22.2 转译：24×24 精灵 RLE 族（`src/game/sprite24.c`）

obj0 里藏着**同一台 24×24 RLE 状态机的 7 份手写副本**（0x4DF84..0x4E29C），
差别只在"流内颜色字节 → 像素"的映射和"透明 token（type 11）"的行为。本 build 的
七个入口（RE_MAP §4 曾把它们笼统归为"24×24 图元"）：

| 转译名 | 原地址 | 颜色映射 | type 11 |
|---|---|---|---|
| `sprite24_ramp` | `0x4DF84` | `base + ((rot + c) & 7)` | 跳过 |
| `sprite24_pal_recolor` | `0x4E016` | `pal[c]` | **把目标已有像素经 pal 重新着色** |
| `sprite24_pal` | `0x4E0A2` | `pal[c]` | 跳过 |
| `sprite24_const` | `0x4E127` | `(u8)stride`（原版把 arg2 低字节当颜色！） | 跳过 |
| `sprite24_ramp24` | `0x4E1A6` | `(c & 7) + 24` | 跳过 |
| `sprite24_plain` | `0x4E22A` | `c` | 跳过 |
| `sprite24_plain49` | `0x4E29C` | `c` | **填 0x49** |

流格式与画面 RLE（`rle.c`）同构（`[2-bit type | 6-bit count-1]`），但固定 24×24、
每行填满才 `dst += stride - 24`；type10 是"逐像素读 1 字节并经映射"（plain 模式即逐字节拷贝）。
原版 7 份循环合并为一台引擎 + 模式结构体。原版入口都是 cdecl（无 usercall 寄存器参数），
可直接经函数指针调用；`sprite24_const` 的颜色确实取自 stride 的低字节（callers 传 `stride|color` 打包值）。

### 22.3 对拍（`src/sprite24check.c` + `build.ps1 -Target sprite24check`）

与 `rlecheck`/`gfxcheck` 同一可信 LE 加载路径；每个模式 300 例随机合法流（每行精确填满、
4 类 token 全覆盖）、随机 stride 24..300、随机 pal/base/rot；目标缓冲带哨兵余量逐字节比对
（recolor 模式两边用相同初值）。

```
build\sprite24check.exe  →  PASS: 2100 cases, 0 failures（首轮即通过）
```

### 22.4 新坑：对拍 exe 自己被 ASLR 放进 guest 窗口 → 低地址预留失败

`sprite24check.exe` 首次运行**必失败**：`le: cannot reserve object region @0x10000: 487`。
真因不是 kernel32，而是**对拍 exe 自己的映像被加载到 `0x30000`**（落在 guest 窗口
`0x10000..0x6FFFF` 内），于是 `le_reserve_address_space_early()` 的强制块被自己占掉。
`gfxcheck`/`rlecheck` 当时映像恰好在高处，所以没暴露。

- **诊断信息误导**：失败后打印的 `0x10000 ... PRIVATE prot=0x40` 是**我们自己已经成功预留的块**
  （`MEM_COMMIT|PAGE_EXECUTE_READWRITE`），不是冲突源；真正冲突块可能在 0x20000..0x6FFFF。
- **修复**：所有走 `le.c` 的 console 对拍目标（`letest`/`rlecheck`/`gfxcheck`/`sprite24check`）
  在 `build.ps1` 加 `/link /BASE:0x60000000`（与 `fd2host` 同一手法）。这与 §8-48 签名 B
  是同源问题；宿主另有 `regress.ps1` 自动重试兜底。

### 22.5 实测判据

```
build\sprite24check.exe →  PASS: 2100 cases, 0 failures
build\gfxcheck.exe      →  PASS: 1450 cases, 0 failures（回归）
build\rlecheck.exe      →  PASS: 1900 cases, 0 failures（回归）
regress.ps1             →  ALL PASS 8/8（fd2host 未改，15 s）
```

### 22.6 下轮入口

1. obj0 工具库剩余：纯字节/调色板变换 `0x4DED4/0x4DEEC/0x4DF09/0x4DF28/0x4DF4C`、
   表访问器 `0x4E7DD..0x4E8BC`、掩码重着色 `0x4E795`（都可续用对拍法）。
2. 连通性/BFS 簇 `0x4E390..0x4E751`（`byte_60068/69` 网格、四方向递归、写回 `dword_60073`）——
   **疑似战场移动范围/寻路**，价值高但有全局状态，转译前先归档数据布局。
3. ★★★ 未动：资源加载器 `sub_111BA`（LMI 容器；内部走 Watcom CRT 的 `fopen/fread`，
   独立对拍需先接宿主 `dos.c` 的文件服务，或改成对目录解析的纯函数）、文本/脚本渲染器 `sub_15F84`。

---

## 23. 第 23 轮：obj0 字节/调色板工具函数转译（2026-10-05）

### 23.1 转译：`src/game/util.c`

第 22 轮把 0x4DED4..0x4E8C0 工具库地形图勾出来了（RE_MAP §4），本轮清掉其中**纯函数**一批：

| 转译名 | 原地址 | 语义 |
|---|---|---|
| `util_rec3` | `0x4DED4` | `base + 3*index`（原版硬编码 base=0x60181；C 版把 base 参数化以便成为数据） |
| `util_translate` | `0x4DEEC` | 就地查表翻译：`buf[i] = table[buf[i]]` |
| `util_sum_tail4` | `0x4DF09` | 求和 `buf[0..n-5]`（原版真地忽略末尾 4 字节，`sub ecx,4`） |
| `util_deobfuscate` | `0x4DF28` | 就地滚动异或：`state=0xA5; state=rol16(state+0x9014,3); buf[i]^=state&0xFF` |
| `util_fix_records` | `0x4DF4C` | 头 `[u8 a][..][u8 b]`，count=a*b；每 4 字节记录：`+3=0xFF, +2&=0x1F, +1&=0x03` |
| `util_mask_recolor` | `0x4E795` | 头 `[u16 w][u16 h][w*h 掩码]`：掩码非 0 时 `dst = pal[dst]`，行步进 stride |

原版循环都是 x86 `loop`/`dec+jnz`（底测 do-while，0 计数会回绕 2³²/2¹⁶ 次），
C 版保留 do-while 形式；游戏数据不会出现 0 计数。`0x4E795` 写 `word_6017B`（C 版镜像为
`util_mask_w`）供对拍断言。

### 23.2 实测坑（都很典型）

1. **`0x4DF09` 改 EBX 却不保存**（其余五个都 `push ebx`）。查调用点：callers 只读 EAX 返回值、
   不依赖 EBX ⇒ EBX 是"死"寄存器，C 版无需复刻；但**对拍时**把它当普通 cdecl 调会让编译器
   的 EBX 状态被踩，`sum` 结果变垃圾。修法：utilcheck 用 `__asm { push ebx; ...; pop ebx }`
   包住原函数调用（`call_sum_orig`）。**将来把转译函数接回宿主时**，这种"原版不守 ABI"的
   函数要留意（我们的 C 版会正常保存 EBX，是安全的超集）。
2. **`0x4E795` 的返回值是"最后一个掩码字节"**（跳过时=0），不是"最后一次调色板值"。
   反汇编里 `xor eax,eax` 后 `lodsb` 每像素写 AL，掩码为 0 时 AL=0。首版误当"最后调色板值"，
   对拍精确抓出（缓冲一致、仅返回值差）。

### 23.3 实测判据

```
build\utilcheck.exe     →  PASS: 2200 cases, 0 failures
build\rlecheck.exe      →  PASS: 1900 cases, 0 failures（回归）
build\gfxcheck.exe      →  PASS: 1450 cases, 0 failures（回归）
build\sprite24check.exe →  PASS: 2100 cases, 0 failures（回归）
regress.ps1             →  ALL PASS 8/8
```

> 注：对拍 exe 仍偶发 `reserve failed`（某个系统 DLL 被 ASLR 放进 guest 窗口，§8-48 签名 B），
> 重跑即过；`/BASE:0x60000000`（§22.4）解决的是"exe 自己"那一种，进程级重试仍是通用兜底。

### 23.4 下轮入口

1. `0x4E390..0x4E751` 连通性/BFS 簇（`byte_60068/69` 网格、四方向递归、写回 `dword_60073`）——
   **优先归档全局数据布局**再转译，疑似战场移动范围/寻路。
2. 表访问器 `0x4E7DD..0x4E8BC`（`&unk_XXXX + 步长*i`，无逻辑，可在需要时批量转成 `base+stride*i`）。
3. ★★★ `sub_111BA`（资源加载，内部走 Watcom CRT 文件服务）、`sub_15F84`（文本/脚本渲染，usercall）。
