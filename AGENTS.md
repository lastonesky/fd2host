# AGENTS.md — AI/协作者工作约定（FD2 → Windows 原生移植）

> 面向接手本仓库的 agent/协作者。**背景知识与事实细节一律看文档**（见文末"文档地图"），
> 本文件只写：怎么干活、怎么验证、什么不能碰。
> 目标与路线见 `README.md`；**当前进度**见 `PROGRESS.md`；踩坑见 `docs/PITFALLS.md`；
> 其余知识/说明/轮次明细按 `docs/INDEX.md` 的分类落盘（2026-10-06 重整过，别再往 README/PROGRESS 里堆）。

---

## 1. 项目速览

把 `E:\FD2\FD2.EXE`（DOS/4GW 32 位保护模式游戏）**原生**跑在 Windows 上：
**不模拟 DOS / 不模拟实模式 / 不用 DOSBox**。路线 C = 32 位二进制宿主（保留原始 x86 游戏逻辑，
平台层换成现代 Win32 实现）→ 逐步源码化。POC 已达成（画面 + 声音 + 进剧情）。

### 最终目标（源码化，一切工作向它收敛）

- 逆向 `FD2.EXE`，最终产出**独立、跨平台的高级语言实现**（C / C++ / C# / Rust / Zig / Go 任选其一，
  不设限）；选型优先级：**代码越简单、生成产物越小、运行越稳定越好**。
- **尊重 exe 编译前的原始逻辑与实现**。理论上无法 100% 反编译回 C，缺口靠 AI 推断 + 逆向证据补齐；
  **前提永远是程序正常运行**，在此之上尽量逼近"人类会写的原始 C 版本"——像还原源码，
  不像反编译器输出。代码要优雅、可读。
- 反编译的坑与资料参考：**https://github.com/wicanr2/fd2_re** 的 `docs/` 与 `docs/knowledge-base/`
  （`git clone` 直接可用；用 curl/wget 抓取则按 §2 第 3 条走代理）。

代码职责（改代码前先定位到正确的文件）：

| 文件 | 职责 |
|---|---|
| `src/le.c/.h` | LE 加载器：对象/页/fixup 重定位、地址空间预留 |
| `src/dos.c/.h` | 平台层：VEH、int/端口接管、DOS/DPMI/BIOS 服务、低内存镜像 |
| `src/render.h` + `render_gdi.c` | **渲染后端接口**与当前 GDI 实现（对拍基准）；`render_sokol.c` = 第 2 步 |
| `src/host.h` + `src/host.c` | **内核**：参数、LE/DOS/AIL 启动、游戏线程、调色板→BGRA、抓帧、watchdog/autokey；不含窗口与消息泵 |
| `src/main_win32.c` | **入口层**：`fd2_entry`、窗口/消息泵/定时器、Win32→BIOS 键盘、`input_post_vk`；`main_sokol.c` = 第 2 步 |
| `src/repl.c` + `src/repl.h` | **源码接入层**：把已验证的转译函数入口改成 5 字节 `jmp rel32` 指向 C 实现（默认全开；`--replace=none\|all\|rle,gfx,sprite24,util,path,dlg,rec,svc,vm,res,bgm,scene,fade,map,fx`）。只对 FD2 build 生效 |
| `src/game/*.c` | 转译产物（`rle`/`gfx`/`sprite24`/`util`/`path`/`res`/`dlg`/`rec`/`svc`）；`src/*check.c` 是各自与原机器码逐字节对拍 |
| `src/ail.c` + `xmidi.c` + `synth.c` + `dls.c` | AIL 替换层 / XMIDI 解析 / 软件合成器 / gm.dls 音色 |
| `src/letest.c` | 加载器自检（对拍 Ghidra 镜像） |
| `re/` | 逆向工作台产物（测绘地图、函数表、静态扫描清单） |

---

## 2. 工作流约定（**必须遵守**）

0. **批量转译节奏（2026-10-08 起，操作者要求）**：一次转译 **~30 个**函数（优先同表/同族的依赖闭合簇），
   **然后每 3-5 个做一次差分验证**（`*check --only=addr,...`），全过后再下一批；
   **假设转译没问题、以批量提速**。若某批 3-5 个里出现失败：
   - 缩到 **1-2 个**重跑 → 定位到具体函数；
   - 改完再回到 3-5 的节奏。
   落地要点：① 对拍 harness 必须支持**按地址选子集**（新批次照 `src/ev2check.c` 的 `--only=` 写）；
   ② 新批次给一个**独立 `REPL_*` 分组**，便于整块 A/B；③ 宿主集成若挂、但对拍全过，
   用 **`FD2_REPL_SKIP=0x...,0x...`**（不需要重建）二分是哪几个在真实运行里出问题；
   ④ 批量写完后 `repl: installed N` 递增应等于批量大小。细则见 `docs/TRANSLATION.md` §1。

1. **随改随写文档**：任何代码/结论/选型一改，**同一轮**就更新对应文档——
   - 行为、命令、对外约定变了 → `README.md`（只放卡片级信息，细节写 `docs/`）
   - 进度（新轮次、下一步计划）→ `PROGRESS.md` 的时间线；**轮次完整病历**写 `docs/rounds/*.md`
   - 二进制事实 / 宿主设计 / 声音 / 后端选型 / 转译方法 → 对应 `docs/BINARY-FACTS.md`、
     `docs/HOST-DESIGN.md`、`docs/AUDIO.md`、`docs/BACKEND.md`、`docs/TRANSLATION.md`
   - 踩坑（根因 + 判据）→ `docs/PITFALLS.md`（编号追加，不删旧条目）
   - 环境/命令/参数 → `docs/ENVIRONMENT.md`；调试手段 → `docs/DEBUG-MANUAL.md`
   - 逆向测绘结论变了 → `re/RE_MAP.md`（FD2）、`re/FDPS_MAP.md`（炎龙外传，已冻结）、`re/funcmap.csv`
   - 归类拿不准就看 `docs/INDEX.md` 的对照表；**不要把内容再堆回 README/PROGRESS**（2026-10-06 重整过）
   禁止"先改代码、以后再补文档"；交接文档的价值就在于及时。
2. **逆向一律走 ida MCP**（首选分析环境），不要用别的反汇编工具重做一遍：
   - 工具：ida MCP（`open_database` / `execute_python` / `reference` / `save_database`）
   - IDA Pro 9.5 + Hex-Rays (x86)；数据库 **`E:\FD2\FD2.EXE.i64`**（直接开 .i64，跳过重新分析）
   - API 陷阱：`ida_hexrays.decompile(ea)`（没有 `idc.decompile`）；`idautils.Strings()` 返回 list；
     `Entries()` 返回 4 元组；**批量结果写文件**（放 `re/`），不要灌进上下文。
   - 批量导出内存/镜像用 **Ghidra 本地 HTTP 桥** `http://127.0.0.1:8089/read_memory`（写文件、不耗上下文）。
3. **联网取 GitHub 内容时用代理**：`git clone` 本身可用；但用 **curl / wget / Invoke-WebRequest**
   之类下载 GitHub（或其它被墙）内容时，**先走本机代理 `http://127.0.0.1:7890`**：
   ```bash
   curl -x http://127.0.0.1:7890 -L -o out.tar.gz https://github.com/...
   # 或全局：export HTTPS_PROXY=http://127.0.0.1:7890  (PowerShell: $env:HTTPS_PROXY=...)
   ```
   下载不到时先试代理，不要反复裸连浪费时间。
4. **实证优先**：结论必须有硬判据（`host.log` 行 / `letest` 逐字节一致 / 截图 / `regress.ps1` 断言 /
   ida 静态证据）。没证实的**标"待确认"**，不要写成事实。禁止"字节扫描/听起来像"式结论
   （教训见 docs/PITFALLS.md §8-25、§12.3）。
5. **git**：仓库根 = `port/`，**`main` 单线开发，不为平台开分支**（平台差异走"单代码库 + 后端选择"）。
   每步一提交，提交前跑回归。
6. **回归**：动了宿主行为就跑 `pwsh -File port\regress.ps1`，以 **8/8 PASS** 为准；
   涉及显示还要同帧 `--screenshot` 对拍。**只在 `build/sandbox` 里做破坏性测试**，
   不要动 `E:\FD2` 下的真实存档（`FD2.SAV` 是原件）。
   **Linux 侧分级（别每轮都拍）**：
   - **每轮**（哪怕只转译游戏逻辑 C）：`make -f Makefile.linux`（构建，0 warning）、
     `letest-linux`（三对象哈希）、`doscheck-linux`（49 断言）—— 秒级，能抓到"编不过/平台缝"。
   - **可选廉价冒烟**：`build/fd2host-linux32 --exit-after=15`（或 `--exit-when-file=FD2.TMP:207360`）
     不抓图、不等 tick600，只确认能跑到 `repl: installed N` + FD2.TMP 尺寸对——约 15 s。
   - **Linux 同 tick 截图对拍（~35 s）只在**：动 `le.c`/`dos.c`/`platform_*`/渲染/输入/入口层/
     `guest_mem`/音频栈，或里程碑节点。纯游戏逻辑转译（bgm/scene/rec…）不必每轮跑：
     新增风险仅是"编不过"（构建已抓），运行期行为由 Windows 的 `*check`+`regress`+A/B 钉住。
   - 抓图一律用 `--exit-when-file=<bmp>:256054` 提前退出，`--exit-after` 只当上限（不要空等到上限）。

---

## 3. 常用命令

```powershell
# 构建（MSVC 14.51 / vcvars32 / 32 位目标）
pwsh -File E:\FD2\port\build.ps1 -Target fd2host     # 宿主（默认 -Render sokol）
pwsh -File E:\FD2\port\build.ps1 -Target letest      # 加载器自检
pwsh -File E:\FD2\port\build.ps1 -Target fd2host -Render gdi   # 换回 GDI 参考实现
# 注意：-Render 是构建期开关（两个入口层都定义 main()，不能共存），没有运行时 --render。
# 两种后端都输出到 build/fd2host.exe，后编覆盖先编；要并存自行 cp 成 fd2host_gdi.exe。

# 沙箱禁止 vcvars 起 reg.exe 时的替代路径（docs/rounds/05-rec-and-services.md §33.5）：
#   build.ps1 在 VSCMD_VER 已设置时不再重复调 vcvars，aux_build.bat 负责把开发者
#   环境变量（含被 vcvars 漏掉的 Windows SDK include/lib）准备好再调 build.ps1。
cmd //c E:\FD2\port\aux_build.bat fd2host
cmd //c E:\FD2\port\aux_build.bat typecheck
cmd //c E:\FD2\port\aux_build.bat vmcheck
cmd //c E:\FD2\port\aux_build.bat platprobe   # 平台自检（内存层）

# 排期依据：按使用量（callers_game + data_xrefs）排未转译的函数（产物 re\func_ranking.csv）
python tools\func_ranking.py --top 30

# 资产导出（游戏资源 → 现代格式）：清单/解包/图像→PNG/音乐→MID；见 docs/ASSETS.md
python tools\fd2assets.py all E:\FD2 --out build\assets
python tools\fd2assets.py image E:\FD2\BG.DAT 3 --gamedir E:\FD2 --out build\assets\bg003.png

# Linux 侧（WSL Debian）：加载器 + 平台自检 + DOS 层自检。跨平台判据 = 两边 `letest` 输出的
# 三个对象 fnv1a 必须逐字相同（参考镜像 build/object*.bin 被 ignore，没有也能判）；
# `doscheck-linux` 与 Windows `doscheck.exe` 同套 49 条断言（故障模型探针：build/faultprobe32，
# freestanding -m32，不需要 gcc-multilib）
wsl -d Debian -- bash -lc "cd /mnt/e/FD2/port && make -f Makefile.linux && ./build/letest-linux /mnt/e/FD2/FD2.EXE /mnt/e/FD2/port/build && ./build/doscheck-linux"
# Linux 宿主：build/fd2host-linux 只证明 POSIX 侧源码全部编译+链接（64 位，跑不了 guest）；
# 真能跑的 host32（-m32，已装 gcc-multilib + i386 的 X11/GL/ALSA）→ build/fd2host-linux32
#   wsl -d Debian -- bash -lc "cd /mnt/e/FD2/port && make -f Makefile.linux host32"

# 运行（WINDOWS 子系统，无控制台；日志恒写 port/build/host.log）
Start-Process E:\FD2\port\build\fd2host.exe -ArgumentList '--exit-after=25' -WorkingDirectory 'E:\FD2'

# 调试/无人值守时压低音量：--volume 默认 100（游戏自己的电平，自己玩不用管），
# 自动跑的都显式加 --volume=10（regress.ps1 已内置）
Start-Process E:\FD2\port\build\fd2host.exe -ArgumentList '--exit-after=25','--volume=10' -WorkingDirectory 'E:\FD2'

# 加载正确性判据（唯一可信）：与 Ghidra 重定位镜像逐字节对比
& E:\FD2\port\build\letest.exe

# 源码转译对拍（原机器码 vs 转译 C，逐字节 + 副作用）
pwsh -File E:\FD2\port\build.ps1 -Target rlecheck; & E:\FD2\port\build\rlecheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target gfxcheck; & E:\FD2\port\build\gfxcheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target sprite24check; & E:\FD2\port\build\sprite24check.exe
pwsh -File E:\FD2\port\build.ps1 -Target utilcheck; & E:\FD2\port\build\utilcheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target pathcheck; & E:\FD2\port\build\pathcheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target rescheck; & E:\FD2\port\build\rescheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target tablescheck; & E:\FD2\port\build\tablescheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target rle2check; & E:\FD2\port\build\rle2check.exe
pwsh -File E:\FD2\port\build.ps1 -Target dlgcheck; & E:\FD2\port\build\dlgcheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target boxcheck; & E:\FD2\port\build\boxcheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target keycheck; & E:\FD2\port\build\keycheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target keyscheck; & E:\FD2\port\build\keyscheck.exe   # 便携键表 vs MapVirtualKeyA（--dump 打全表）
pwsh -File E:\FD2\port\build.ps1 -Target reccheck; & E:\FD2\port\build\reccheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target typecheck; & E:\FD2\port\build\typecheck.exe
pwsh -File E:\FD2\port\build.ps1 -Target vmcheck; & E:\FD2\port\build\vmcheck.exe
# 批量批次的对拍器（**支持按地址选子集**，一次验 3-5 个）：
#   --only=0x35298,0x35321,...   --cases=N（默认 200）
pwsh -File E:\FD2\port\build.ps1 -Target ev2check; & E:\FD2\port\build\ev2check.exe --only=0x135DD,0x35298,0x35321,0x353B5,0x353E7
# DOS 层跨平台自检（低内存镜像 + INT 21h 文件服务 + 真 int 0x21 经故障入口分发）：
# 两平台跑同一套 49 条断言，必须 49/49 + exit 0（docs/rounds/15-dos-and-faults.md）
pwsh -File E:\FD2\port\build.ps1 -Target doscheck; & E:\FD2\port\build\doscheck.exe

# 一键回归（重建沙箱、删 FD2.TMP、autokey 走 continue、8 项断言）
pwsh -File E:\FD2\port\regress.ps1

# 抓帧（不依赖窗口/桌面，核对调色板/通道序）
Start-Process E:\FD2\port\build\fd2host.exe `
  -ArgumentList '--exit-after=30','--screenshot=E:\FD2\port\build\frame.bmp','--shot-frame=700' `
  -WorkingDirectory 'E:\FD2'
```

常用参数（全部支持 `--opt value` 与 `--opt=value` 两种写法）：
`--gamedir`、`--exe`、`--exit-after <秒>`、`--exit-when-file=<路径>:<字节数>`（文件写满且 autokey
跑完 → 提前干净退出；与 `--exit-after` 上限配合）、`--replace=none|all|groups`（默认 `all`：
接入已对拍的转译函数；`none` 用于 A/B）、`--headless`、`--trace=<n>`（单步跟踪）、
`--screenshot=<bmp>`（**必须绝对路径**：宿主会 chdir 到游戏目录，相对路径静默写不出图）
配 `--shot-frame=<n>`（按帧号，仅固定帧率下有意义）/ `--shot-time=<ms>`（按墙钟）
/ **`--shot-tick=<n>`**（按游戏 BIOS tick，**跨后端对拍用这个**，见 `docs/BACKEND.md` §13.8）、
`--autokey=<延时ms:VK[,VK...];...>`（无人值守按键回归）、
`--midi-dump=<wav>`（离线核对音乐）、`--ail-dump=<dir>`、`--midi-test`、`--gm-bank=<path>`。

⚠ **跑完先看日志里的 `host: working directory = …`**：参数没被识别时是**静默回退**到
`E:\FD2`，不报错（曾让对照实验跑错目录，见 docs/PITFALLS.md §8-32）。

---

## 4. 硬约束（碰了必炸，改代码前先对照）

- **地址空间布局不可随意改**（docs/HOST-DESIGN.md §4.1）：游戏对象占 `0x10000..0x6FFFF`、
  低内存镜像 `0x70000..0x7FFFF`、VGA `0xA0000`；低 64 KiB 不可映射。
- **宿主映像必须小（现在 ~288 KB）且保留 ASLR**（`/DYNAMICBASE` + `/BASE:0x60000000`）：
  大静态数组（如 8 MB buffer）或关 ASLR 都会把游戏地址空间挤掉——大块内存一律 `VirtualAlloc`/`malloc`。
- **地址空间预留必须在 CRT 之前**：自定义入口 `fd2_entry`（`/ENTRY:fd2_entry`），在 `main()` 里做已太晚。
- **不许按字节扫描改写游戏代码**（`CD xx` 之类）：会改坏 `call` 位移（§8-25 的"continue 就退出"）。
  int 接管走 VEH 读 `[EIP]==0xCD` 直接分派，**保持游戏快照与镜像逐字节一致**。
- **INT 21h/31h 的返回值语义错一个就跑飞**（§4.4 有完整语义表）：改 `dos.c` 前先读该表 + §8。
- **工作目录必须是游戏目录**（游戏按裸文件名开资源）。
- AIL 的 `*.DIG`/`*.MDI` 是 16 位实模式代码，**任何路线下都整体替换**，不要尝试执行。

## 5. 调试速查

| 需求 | 做法 |
|---|---|
| 看运行日志 | `port/build/host.log`（唯一运行观测口） |
| 验证加载 | `letest.exe`（obj1/obj2 必须逐字节一致） |
| 崩溃地址 → 符号 | `port/fd2host.map`（RVA = 地址 − 映像基址） |
| 跟丢执行流 | `--trace=<n>`（VEH 置 TF 单步） |
| 抓画面证据 | `--screenshot`（绝对路径）+ `--shot-frame` / `--shot-time` / `--shot-tick`；**抓完即退**加 `--exit-when-file=<该BMP>:256054`（否则干等到 `--exit-after`，单轮 60 s → 22 s）。BMP→PNG 用 `python tools\bmp2png.py in.bmp out.png`（或 `.Save($p,[System.Drawing.Imaging.ImageFormat]::Png)` —— `Save(路径)` 存的是原图格式，会出花屏）。跨后端比画面用 `--shot-tick`（`docs/BACKEND.md` §13.8）；过渡段抓图要重试（`§8-55`） |
| 无人值守菜单路径 | `--autokey=...`（例：`5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN` 走 continue） |
| **复现用户手玩的操作** | 用户跑 `--keylog=<文件>` → 你用 `--keyplay=<同文件>` 重跑（**绝对时间**，与 `--autokey` 不通用）；不给参数时 `host.log` 里也有每键一行与收尾的 `host: key schedule (…)`。录制↔回放同 tick 抓帧 **0 px**（`docs/rounds/12-keylog.md`）；启动瞬间会混入别处击键，见 `§8-57` |
| 平台层还缺什么 | ida MCP 对照静态清单 `re/int21_ah_used.txt`、`re/int_sites_all.txt` vs `src/dos.c` 的 `switch (ah)`；日志会打印前 40 条 `UNHANDLED INT21` |
| 游戏自己挂 INT 9 / 按键无效 | 日志链：`dos: INT 9 vector :=`（挂上）→ `INT 9 queued scan=` → `INT 9 injected ... esp →`（栈必须配平）；画面判据：`--screenshot` 前后两张图对比（§18.4）。若 `pop ds` 处 #GP，先看 `isr: handler bytes:` 是否被 fixup 踩过（§8-49） |
| **换游戏前先体检（不运行）** | `python re\preflight.py <exe>`（LE/对象表/与预留区冲突/AIL 特征）+ `python re\fixup_scan.py <exe>`（fixup 语法要 `bad=0 leftover=0`）；然后 `--exe <新exe> --gamedir <新目录>` 跑，看日志 `fixups applied / low-memory window moved / INT10 / DAC`（§14） |
| 游戏 `spawn`/`exec` 另一个 EXE | `INT 21h AH=4B` 已实现（§16）：子进程是另一个 `fd2host.exe`，日志在 **`port/build/host.<pid>.log`**（父日志 `host.log`）；尾巴看 `dos: PSP:0x80 command tail`，子进程退出码看父日志 `child exited with N` |
| 游戏自己不报错也没画面 | 看 `dos: write h=1 ... n=` 是否为 0（游戏 printf 被丢，§8-35）；看“端口操作数”是否暴涨到几千万（`0x3DA` 死循环，§8-38）；看是否卡在 `AIL_register_timer`（回调不触发，§14.5） |
| 手工复现 fresh install | 数据文件拷到任意目录 + **删 `FD2.TMP`** → `--gamedir <该目录> --autokey=...` |
| 反汇编/反编译游戏函数 | ida MCP（主）；Ghidra HTTP 桥 `/read_memory`、`/list_segments`（批量） |
| **对拍依赖文件/内存的游戏函数** | **CRT 重定向术**（见 `docs/TRANSLATION.md` §2 及 `docs/rounds/03-tables-and-plumbing.md` §25.2）：`le_map_and_relocate` 后把 CRT 入口 `0x3706E/0x3776E/0x37324/0x3759C/0x37940/0x373CA` 头 5 字节改成 jmp 到宿主 libc 封装，再直接调原机器码——不碰游戏逻辑，不需要 DOS 层。样例见 `src/rescheck.c` |

## 6. 常见坑（速查，完整清单见 docs/PITFALLS.md §8，**动手前先通读 §8**）

- 命令行参数写法不匹配 = 静默用默认值（§8-32）；特权指令模拟要返回**指令长度**（§8-13）；
  调色板 6 位 DAC + DIB 是 BGRA（§8-17）；DOS 的"写 0 字节 = 截断"在 Windows 是空操作（§8-31）。
- `.gitignore` 两个陷阱（§13.5）：`*.dll`/`*.lib`/`x86/` 会挡住 vendored 三方库（需 `!vendor/**`）；
  `build/object*.bin`（letest 参考镜像）被 `*.bin` 忽略——新克隆用 **`python tools/ghidra_objects.py`**
  从本地 Ghidra 桥一键重导（或按 §2 方法手动导出）。**没有参考文件也能判**：`letest` 会打印
  三个对象的 `fnv1a`，两个平台哈希相同即证明加载器一致；有参考时应为
  `reference check OK, exact match`（出现 `explained differences only` 就是有真分歧，
  用 `tools/fixup_dump.py` 查 fixup，见 `docs/rounds/14-fixup-boundary.md`）。
- **对拍 exe 自己被 ASLR 放进 guest 窗口**（§22.4）：新增任何走 `le.c` 的 console 对拍目标，
  必须在 `build.ps1` 给它 `/link /BASE:0x60000000`；否则 exe 映像可能落在 `0x10000..0x6FFFF`，
  `le_reserve_address_space()` 失败且报错信息会指向自己已预留的 0x10000（误导）。
- 计划状态会过期：`PROGRESS.md`「下一步计划」的勾选项以正文实测为准；发现文档与代码不符，**当场修文档**。

## 7. 文档地图

| 文档 | 内容 |
|---|---|
| `README.md` | **项目卡片**：目标、路线、目录树、快速开始、关键结论一句话版、下一步摘要 |
| `PROGRESS.md` | **只看进度**：一句话现状、能力清单、轮次时间线、下一步计划（含 § 编号沿用说明） |
| `docs/INDEX.md` | **总导航**：自建文档清单 + 旧 `PROGRESS.md` 的 §编号 → 新文件对照表 + 按需求查表 |
| `docs/ENVIRONMENT.md` | 工具链、构建/运行、**全部命令行参数**、Ghidra HTTP 桥、IDA MCP（旧 §2 §10） |
| `docs/BINARY-FACTS.md` | `FD2.EXE` 容器、LE 头实测偏移、fixup 格式、对象布局（旧 §3） |
| `docs/HOST-DESIGN.md` | 地址空间硬约束、源文件职责、VEH 四类异常、服务返回值语义（旧 §4） |
| `docs/PITFALLS.md` | **踩坑清单（动手前必读）** + 历史卡点（旧 §8 §6） |
| `docs/DEBUG-MANUAL.md` | 诊断手段与自检/对拍工具清单（旧 §9） |
| `docs/AUDIO.md` | AIL 替换层、XMIDI、合成器、gm.dls、已知杂音 bug（旧 §11） |
| `docs/BACKEND.md` | sokol 选型实测、git 策略、跨平台抽取顺序（旧 §13） |
| `docs/TRANSLATION.md` | 源码转译方法 + 模块/对拍/接入清单 + 下一步（旧 §19–§33 提炼） |
| `docs/ASSETS.md` | 资产导出：`tools/fd2assets.py`（容器解包 / 图像→PNG / XMIDI→MID）+ 已覆盖与待接格式清单 |
| `docs/FDPS-ARCHIVE.md` | FDPS 炎龙外传**冻结存档**（旧 §14–§18） |
| `docs/rounds/*.md` | 逐轮病历：01 平台/工具、02 叶子工具层、03 地基管线、04 对话框 UI、05 记录与服务 |
| `re/RE_MAP.md` | 逆向测绘地图（FD2）：函数分区、AIL 边界、核心函数档案、转译路线 |
| `re/FDPS_MAP.md` | 逆向测绘地图（FDPS 炎龙外传）：90 条 AIL 入口表、定时器族、spawn FD.EXE 流程 |
| `re/funcmap.csv` | 全量函数表（1359 行） |
| `re/*.txt` / `re/*.c` | 静态扫描清单与关键函数反编译存档 |
| 外部：github.com/wicanr2/fd2_re（`docs/`、`docs/knowledge-base/`） | 反编译踩坑与知识库（同游戏逆向资料）。**本地已 curate 快照 `port/docs/knowledge-base/` + `port/docs/data/`（被 `.gitignore` 忽略，约 9 MB/274 文件；取舍理由与清单见 `docs/KEEP.md`；注意 `docs/*.md` 与 `docs/rounds/` 是**本项目的自建文档，要入库**）**：保留 `knowledge-base/`、`data/ida/*.txt`、`data/exe_tables`、游戏数据 JSON；但它**是另一个 FD2.EXE build**（md5 `b97caf22…`，非本项目 `a6e341a8…`）——地址/常量/指令一律以 `E:\FD2\FD2.EXE.i64` 复核（docs/rounds/02-translation-toolkit.md §21.1） |
