# 调试手册与诊断工具
可用的诊断手段： `letest` / 各 `*check` 对拍 exe / 抓帧 / 崩溃转储 / 单步跟踪 / 差分脚本。
对应旧 `PROGRESS.md` §9，并汇总了原 README「调试手法」一节。
---

## 9. 调试手册

| 需求 | 做法 |
|---|---|
| 看宿主运行日志 | `port/build/host.log`（WINDOWS 子系统，不弹控制台） |
| 验证加载是否正确 | `letest.exe`（与 `build/object1..3.bin` 逐字节对比） |
| 重新导出 Ghidra 重定位镜像 | HTTP 桥 `/read_memory` 循环写文件（见 §2） |
| 反查崩溃地址属于哪个函数 | `port/fd2host.map`（RVA = 地址 - 映像基址；section:offset = RVA - 0x1000） |
| 跟丢执行流 | `--trace=<n>` 单步跟踪（VEH 里置 TF，注意每次单步异常后要重新置位） |
| 看谁改了内存 | 崩溃报告会打印 EIP 前后 48 字节 + 分配账本（`note_alloc`） |
| 反汇编游戏函数 | Ghidra：`decompile_function` / `read_memory`；注意 obj0 是 32 位平坦代码，obj1/obj2 是数据 |
| 已知的 Ghidra 陷阱 | `_entry`(0x3CCB4) 的反编译里充满 `in_DS/in_ES/swi()` 伪寄存器——那是段寄存器访问与 `int` 指令的建模，代码本身是正常的 32 位代码 |
| 抓当前帧画面（不依赖窗口/桌面） | `--screenshot=<绝对路径> --shot-frame/--shot-time/--shot-tick=<n>`，日志出现 `host: frame N dumped … (age … ms, guest tick …)`。**要抓完就退出**就再加 `--exit-when-file=<该BMP>:256054`（BMP 写满 +2 s 即退，单轮 22 s；只给 `--exit-after=60` 会在画面上白等几十秒）。**`--screenshot` 必须用绝对路径**（宿主会 chdir 到游戏目录，相对路径只在日志留一行 `host: cannot write frame dump …`）。帧数见 watchdog 行 `(N frames drawn) - X fps`。帧内容也可用 ASCII 网格打印（不依赖看图工具）：对 `GetPixel` 采样 64×24、按亮度映射成 ` .:-=+*#%@` |
| **抓“打字进行中”画面**（证明 `vm_run`+`dlg_type_step` 在宿主里跑过） | 标准 autokey + `--shot-tick=326..334`（tick 320 还没开框、335 已打完）+ `--exit-when-file` 即时退出；**过渡段跨运行会错位，要重试 + 用像素判据挑帧**（§8-55）。配方与判据见 `docs/rounds/10-typewriter-recipe.md` §40.2 |
| **跨渲染后端比同一画面** | 用 **`--shot-tick=<n>`**（按游戏 BIOS tick `0x40:0x6C`，与帧率无关）。不要用 `--shot-frame`（帧号在不同帧率下不是同一时刻），`--shot-time` 也只到 ±1 个帧周期（GDI ±31 ms / sokol ±6 ms）的精度。见 `docs/BACKEND.md` §13.8 |
| **换一个游戏前的静态体检** | `python re\preflight.py <exe>`（LE/对象布局/与宿主预留区冲突/AIL 特征/扩展器）+ `python re\fixup_scan.py <exe>`（fixup 语法，要 `bad=0 leftover=0`）。两个都不运行、零风险，能在开跑前报出必修点（§14.1） |
| 看不到游戏自己的文本 | 先看日志里 `dos: write h=1 ... n=` 是不是 0（句柄无效 = 游戏 printf 全丢，§8-35）；`AH=3F/40` 对 ≤512 字节的小传输有内容日志（前 40 条） |
| 游戏停在第一帧 / 端口操作数暴涨 | `0x3DA` 状态位没翻转（等回扫的经典写法死循环，§8-38）；端口操作数是正常量级的百倍/千倍即是此病 |
| 崩溃现场新增字段 | AV 转储现在含 `RLE w/h (@0x627B4)`、`[ESI]` 源字节、`[ESP]` 返回地址、EBP 帧的 6 个参数 —— 定位"解压写飞"与"分配器越界"两类问题最快 |
| 文件写入回归（一键） | `pwsh -File port\regress.ps1`：重建 `build\sandbox`（删掉 `FD2.TMP`）→ `--autokey` 走 continue → 对日志+文件系统断言 8 项，`ALL PASS` 为准（§12.4） |
| **转译接入的 A/B 画面证据** | 同一 `--autokey` + 固定 `--shot-frame` 分别以 `--replace=none` / `all` 抓帧 → `pwsh -File port\framediff.ps1 -A <none.bmp> -B <all.bmp>`；差值必须 ≤ none↔none 基线（对话框帧的 autokey/帧号与实测数据见 §30.3） |
| 手工复现 fresh install | 把数据文件拷到任意目录、**删掉 `FD2.TMP`**，再 `--gamedir <该目录> --autokey=5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN` |
| 查平台层还缺哪些服务 | ida MCP 裸扫 obj0 的 `CD xx` + 回看 `int 21h` 前的 `mov ah,imm`：产物 `re/int_sites_all.txt`、`re/int21_ah_used.txt`；宿主侧对照 `src/dos.c` 的 `switch (ah)` |

---

## 9A. 自检 / 对拍工具清单

**唯一的正确性判据是"与反汇编器镜像或原始机器码逐字节一致"**，不要靠肉眼代码如下 navigate。

| 工具 | 判据 | 备注 |
|---|---|---|
| `letest.exe` | 加载结果 vs Ghidra 重定位镜像逐字节 | 加载正确性的**唯一可信判据** |
| `rlecheck` / `gfxcheck` / `sprite24check` / `utilcheck` / `pathcheck` / `tablescheck` / `rle2check` / `reccheck` | 转译 C vs **原始机器码**逐字节（含全局副作用） | 用例数见 `TRANSLATION.md` §4 |
| `rescheck` | CRT 重定向后调原版 `0x111BA` vs 转译 C | CRT 重定向术（`TRANSLATION.md` §2） |
| `boxcheck` | 逐帧 VGA + 5 段快照 + **事件序列** + 每次 delay 抓帧 | 连调用顺序都对拍 |
| `keycheck` / `typecheck` | 低内存镜像 + **确定性时钟** | 原机器码与转译 C 共用同一个时钟桩（每读一次 tick 加一） |
| `vmcheck` | 脚本 VM `0x15F84`：**完整事件序列**（12 个被调函数全桩化）+ 全局 + 返回值 | 词流由合法生成器产生；`VMONLY=<id>` `VMTRACE=1` 取单例现场（定位崩溃/差异用） |
| `framediff.ps1` | 两个 `--screenshot` BMP 的逐像素差 | repl A/B：差值必须 ≤ none↔none 基线噪声 |
| `re/preflight.py` / `re/fixup_scan.py` | 不运行的静态体检（LE/对象布局/与预留区冲突/AIL 特征；fixup 要 `bad=0 leftover=0`） | 换游戏先跑这个 |
| `tools/xmi_cc7.py` | 扫 XMIDI `EVNT`，按声道统计 CC7（`--ail-dump` 或直接切 `FDMUS.DAT` 的 FORM/XMID） | 判定“原版 `AIL_set_sequence_volume` 的淡变覆盖哪些声道”——原版只乘 CC7（§35.2④） |

其它要点：

- 所有走 `le.c` 的 console 对拍 exe 都链 **`/BASE:0x60000000`**（否则自身映像被 ASLR 放进 guest
  窗口导致低地址预留失败，§22.4）。
- 崩溃转储会打印：EIP 前后 48 字节、`RLE w/h (@0x627B4)`、`[ESI]` 源字节、`[ESP]` 返回地址、
  EBP 帧的前 6 个参数、全部 INT21/INT31 分配块、最后被接管的中断站点；
  `port/fd2host.map` 可把宿主 RVA 反查成符号。
- `--screenshot` 的 BMP→PNG：**用仓库现成工具** `python tools\bmp2png.py in.bmp out.png`
  （顺带打印 bbox/ink）；或 `[System.Drawing.Image]::FromFile($bmp).Save($png,
  [System.Drawing.Imaging.ImageFormat]::Png)` —— **注意 `Save(路径)` 存的是原图格式**（产物头
  是 `BM`，扩展名 `.png` 会骗过文件名、骗不过看图器 ⇒ 花屏，§40 踩过）；
  画面内容也可用 ASCII 网格打印（64×24 采样 + 亮度映射 ` .:-=+*#%@`），不依赖看图工具。
