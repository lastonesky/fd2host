# 环境与工具链
编译/运行环境、构建与运行命令、**宿主全部命令行参数**、外部工具桥（Ghidra HTTP 桥）、
IDA Pro + ida MCP 逆向工作台。对应旧 `PROGRESS.md` §2、§10。

> 2026-10-06 文档重整时由 `PROGRESS.md` 拆出：`PROGRESS.md` 只留进度时间线，
> 说明/知识/经验类内容按主题落在 `docs/` 下。§ 编号沿用旧编号，对照表见 `docs/INDEX.md`。
---

## 2. 环境、工具链、复现命令

| 项 | 值 |
|---|---|
| 编译器 | MSVC 14.51（VS 18 Enterprise），`C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat` |
| 目标 | 32 位 x86（`x86:LE:32`），Windows SDK 10.0.26100 |
| 反汇编 | Ghidra 12.0.4 @ `E:\Dev\ghidra_12.0.4_PUBLIC`；ghidra-mcp @ `E:\Dev\ghidra-mcp` |
| 游戏目录 | `E:\FD2`（宿主必须以此为工作目录） |

```powershell
# 构建全部/单个目标
pwsh -File E:\FD2\port\build.ps1 -Target fd2host
pwsh -File E:\FD2\port\build.ps1 -Target letest

# 运行（宿主是 WINDOWS 子系统，不弹控制台；日志写入 port/build/host.log）
Start-Process E:\FD2\port\build\fd2host.exe -ArgumentList '--exit-after=25' -WorkingDirectory 'E:\FD2'

# 加载器自检：把加载结果与 Ghidra 导出的重定位镜像逐字节对比（加载正确性的唯一可信判据）
& E:\FD2\port\build\letest.exe
```

**所有带值的参数都同时支持 `--opt value` 与 `--opt=value` 两种写法**（`host_init()` 统一归一化，
另一种写法不再静默回退到默认值，见 `PITFALLS.md` §8-32/§8-33）。

### 全部命令行参数（汇总）

| 参数 | 作用 |
|---|---|
| `--gamedir <dir>` | 游戏工作目录（游戏按裸文件名开资源，跑 `-WorkingDirectory` 之外时用） |
| `--exe <path>` | 跑别的 LE 游戏（见 `FDPS-ARCHIVE.md` §14）；配合 `re/preflight.py` 先做静态体检 |
| `--exit-after <秒>` | 到点后 watchdog 触发干净退出 |
| `--exit-when-file=<路径>:<字节数>` | 文件写满且 autokey 跑完 → 提前干净退出（+2 s 缓冲，退出前抓最后一帧）；与 `--exit-after` 上限配合，回归单次 ~15 s（§20） |
| `--trace=<n>` | 单步跟踪（VEH 置 TF） |
| `--headless` | 不画帧（只要逻辑） |
| `--image` | 用预导出镜像代替解析 exe |
| `--screenshot=<file.bmp>`（**用绝对路径**，宿主会 chdir 到游戏目录） | 导出**实际送显**的 RGB 缓冲，不依赖窗口/桌面 |
| `--shot-frame=<n>` | 按**帧号**触发抓帧（默认 300）。只在固定帧率下有意义，跨后端不可用 |
| `--shot-time=<ms>` | 按**墙钟**触发（`host_init` 起的毫秒）。精度 = ±1 个帧周期（GDI ±31 ms / sokol ±6 ms） |
| **`--shot-tick=<n>`** | 按**游戏自己的 BIOS tick**（`0x40:0x6C`，18.2 Hz 独立推进）触发。**跨后端对拍用这个**——同一 tick = 同一 guest 状态，与帧率无关（见 `BACKEND.md` §13.8） |
| `--autokey=<延时ms:VK[,VK...];...>` | 无人值守按键序列（菜单路径回归） |
| `--replace=none\|all\|rle,gfx,sprite24,util,path,dlg,rec,svc,vm,res` | 是否把已对拍的转译函数接进游戏（默认 `all`）；`none` 用于 A/B（见 `TRANSLATION.md`）。分组名与 `src/repl.c` 的 `REPL_*` 一一对应 |
| `--no-user-input` | 忽略真实键鼠，避免测试机被使用时干扰 autokey |
| `--cmdtail=<尾巴>` | 写进 `PSP:0x80` 的命令行；`INT 21h AH=4B` 拉起子进程时自动传递 |
| `--log=<路径>` | 换日志文件；子进程各用各的 `host.<pid>.log`，否则会截掉父日志（§8-47） |
| 音频：`--ail-dump=<dir>` / `--ail-rate=<Hz>` / `--ail-bits=<8\|16>` / `--ail-stereo` | 导出音效样本与 XMIDI 原始数据、指定格式（见 `AUDIO.md`） |
| **`--volume=<0..100>`** | 总输出音量（音乐 + 音效，**在送进混音器前衰减**）。**默认 `100` = 游戏自己的电平、不衰减**，这就是加 `--volume` 之前的听感；调试/回归时显式压低，例如 `--volume=10`（`regress.ps1` 已内置）。`0` 仍会跑完整条音频流水线，只是静音（实测 `--audio-dump` 稳态 RMS 396 → 4081 = 10.3×） |
| **`--audio-rate=<Hz>`** | 混音器设备采样率，默认 **22050**（= 音乐原生采样率，音乐就不必重采样）；`audio.h` 的设备只能开一次（`saudio_setup` 有 assert），所以采样率不能“试几个”，要用它指定 |
| **`--audio-dump=<wav>`** | 把**混音器交给设备的那串样本**录成 WAV（音乐+音效混合后、实音量），用于离线量化音频（逐秒 RMS / 音量语义 / 音效突发）—— 音频判据不再靠耳朵 |
| **`--keylog=<路径>`** | 录制按键：每个 make 码立刻追加一行 `<自启动的毫秒>:<键名>`（逐条 `fflush`，**崩溃也留得下**）；相对路径落在 `host.log` 同目录。不给此参数也照记——每键一行 `host: key @ms KEY` 进 `host.log`，收尾打一条完整的 `host: key schedule (…)` |
| **`--keyplay=<路径>`** | 回放上述录制（**绝对时间**，与 `--shot-time`/`--exit-after` 同一基准）。**与 `--autokey` 不通用**：autokey 的延时是相对上一步的，录制文件的时间是绝对的 ⇒ 用本参数；两者同时给时本参数优先。退出触发（`--exit-when-file`）会等回放结束 |

**一键回归**：`pwsh -File E:\FD2\port\regress.ps1`（重建沙箱 → autokey → 8 项断言，见
`rounds/01-platform-and-tooling.md` §12.4）。

### 关键外部工具：Ghidra 本地 HTTP 桥

Ghidra 进程（`javaw`）在 **`http://127.0.0.1:8089`** 上开了一个 HTTP 桥，实测可用端点：

```powershell
Invoke-WebRequest "http://127.0.0.1:8089/read_memory?address=0x10000&length=300"
Invoke-WebRequest "http://127.0.0.1:8089/list_segments"
Invoke-WebRequest "http://127.0.0.1:8089/list_methods"
Invoke-WebRequest "http://127.0.0.1:8089/decompile?address=0x3ccb4"   # 端点名待确认
```

**用途**：`/read_memory` 可批量导出内存并直接写文件，**不消耗模型上下文**——比走 MCP 工具高效得多
（`build/object1..3.bin` 就是这么导出的，作为重定位参考镜像）。
注意：`run_script_inline` 等脚本接口被禁用（需要 `GHIDRA_MCP_ALLOW_SCRIPTS=1`）。

---

## 10. IDA Pro 逆向环境（ida MCP，2026-10-04 建立）

**源码转译的主分析环境**。完整测绘见 **`port/re/RE_MAP.md`**（函数分区、AIL 边界、核心函数档案、
转译路线），全量函数表见 `port/re/funcmap.csv`（1359 行）。

| 项 | 值 |
|---|---|
| 工具 | ida MCP（`use_capability` → `mcp-tool:ida/open_database \| execute_python \| reference \| save_database`） |
| IDA | IDA Professional 9.5 + Hex-Rays (x86) |
| 数据库 | `E:\FD2\FD2.EXE.i64`（4.4 MB，已含 51 个 AIL 函数重命名）；直接打开 .i64 可跳过重新分析 |
| 第二个数据库 | `E:\Games\FDCollection\Game\FDPS\FDPS.EXE.i64`（2026-10-05 由 ida MCP `open_database` 自动分析并 `save_database` 生成，**无需手动在 IDA GUI 里加载**：1353 函数、437 串、入口 `start`=0x43008，与 `preflight.py` 一致） |
| 已验证 | IDA 段布局/入口与 Ghidra **逐项一致**（cseg01=0x10000、入口 0x3CCB4），fixup 已被 IDA 应用 |

**一句话结论**：游戏区 569 个函数（0x10000..0x37000）**零条 `int` 指令**，DOS 交互全走 CRT 包装
（`int386`/`fopen`/`sbrk`…）⇒ 分层转译成立：游戏逻辑 = 纯计算 + `0xA0000` 显存访问；平台层只需给
CRT 提供 Win32 实现；Miles AIL 的 51 个 `AIL_*` 入口整体打桩替换。

**IDA 9.5 API 陷阱**：用 `ida_hexrays.decompile(ea)`（**没有** `idc.decompile`）；`idautils.Strings()`
返回 list（无 `.setup()`）；`idautils.Entries()` 返回 4 元组；批量分析结果**写文件**而不是返回上下文。
