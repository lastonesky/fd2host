# AGENTS.md — AI/协作者工作约定（FD2 → Windows 原生移植）

> 面向接手本仓库的 agent/协作者。**背景知识与事实细节一律看文档**（见文末"文档地图"），
> 本文件只写：怎么干活、怎么验证、什么不能碰。
> 目标与路线见 `README.md`；进度、踩坑、实测数据见 `PROGRESS.md`。

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
| `src/ail.c` + `xmidi.c` + `synth.c` + `dls.c` | AIL 替换层 / XMIDI 解析 / 软件合成器 / gm.dls 音色 |
| `src/letest.c` | 加载器自检（对拍 Ghidra 镜像） |
| `re/` | 逆向工作台产物（测绘地图、函数表、静态扫描清单） |

---

## 2. 工作流约定（**必须遵守**）

1. **随改随写文档**：任何代码/结论/选型一改，**同一轮**就更新对应文档——
   - 行为、命令、对外约定变了 → `README.md`
   - 进度、根因、修复、实测数据、踩坑 → `PROGRESS.md`（对应小节，没有就新开一节并编号）
   - 逆向测绘结论变了 → `re/RE_MAP.md`、`re/funcmap.csv`
   禁止"先改代码、以后再补文档"；交接文档的价值就在于及时。
2. **逆向一律走 ida MCP**（首选分析环境），不要用别的反汇编工具重做一遍：
   - 工具：ida MCP（`open_database` / `execute_python` / `reference` / `save_database`）
   - IDA Pro 9.5 + Hex-Rays (x86)；数据库 **`E:\FD2\FD2.EXE.i64`**（直接开 .i64，跳过重新分析）
   - API 陷阱：`ida_hexrays.decompile(ea)`（没有 `idc.decompile`）；`idautils.Strings()` 返回 list；
     `Entries()` 返回 4 元组；**批量结果写文件**（放 `re/`），不要灌进上下文。
   - 批量导出内存/镜像用 **Ghidra 本地 HTTP 桥** `http://127.0.0.1:8089/read_memory`（写文件、不耗上下文）。
3. **联网取 GitHub 内容时用代理**：`git clone` 本身可用；但用 **curl / wget / Invoke-WebRequest**
   之类下载 GitHub（或其它被墙）内容时，**先走本机代理 `http://127.0.0.1:7980`**：
   ```bash
   curl -x http://127.0.0.1:7890 -L -o out.tar.gz https://github.com/...
   # 或全局：export HTTPS_PROXY=http://127.0.0.1:7890  (PowerShell: $env:HTTPS_PROXY=...)
   ```
   下载不到时先试代理，不要反复裸连浪费时间。
4. **实证优先**：结论必须有硬判据（`host.log` 行 / `letest` 逐字节一致 / 截图 / `regress.ps1` 断言 /
   ida 静态证据）。没证实的**标"待确认"**，不要写成事实。禁止"字节扫描/听起来像"式结论
   （教训见 `PROGRESS.md` §8-25、§12.3）。
5. **git**：仓库根 = `port/`，**`main` 单线开发，不为平台开分支**（平台差异走"单代码库 + 后端选择"）。
   每步一提交，提交前跑回归。
6. **回归**：动了宿主行为就跑 `pwsh -File port\regress.ps1`，以 **8/8 PASS** 为准；
   涉及显示还要同帧 `--screenshot` 对拍。**只在 `build/sandbox` 里做破坏性测试**，
   不要动 `E:\FD2` 下的真实存档（`FD2.SAV` 是原件）。

---

## 3. 常用命令

```powershell
# 构建（MSVC 14.51 / vcvars32 / 32 位目标）
pwsh -File E:\FD2\port\build.ps1 -Target fd2host     # 宿主
pwsh -File E:\FD2\port\build.ps1 -Target letest      # 加载器自检

# 运行（WINDOWS 子系统，无控制台；日志恒写 port/build/host.log）
Start-Process E:\FD2\port\build\fd2host.exe -ArgumentList '--exit-after=25' -WorkingDirectory 'E:\FD2'

# 加载正确性判据（唯一可信）：与 Ghidra 重定位镜像逐字节对比
& E:\FD2\port\build\letest.exe

# 一键回归（重建沙箱、删 FD2.TMP、autokey 走 continue、8 项断言）
pwsh -File E:\FD2\port\regress.ps1

# 抓帧（不依赖窗口/桌面，核对调色板/通道序）
Start-Process E:\FD2\port\build\fd2host.exe `
  -ArgumentList '--exit-after=30','--screenshot=E:\FD2\port\build\frame.bmp','--shot-frame=700' `
  -WorkingDirectory 'E:\FD2'
```

常用参数（全部支持 `--opt value` 与 `--opt=value` 两种写法）：
`--gamedir`、`--exe`、`--exit-after <秒>`、`--headless`、`--trace=<n>`（单步跟踪）、
`--screenshot=<bmp> --shot-frame=<n>`、`--autokey=<延时ms:VK[,VK...];...>`（无人值守按键回归）、
`--midi-dump=<wav>`（离线核对音乐）、`--ail-dump=<dir>`、`--midi-test`、`--gm-bank=<path>`。

⚠ **跑完先看日志里的 `host: working directory = …`**：参数没被识别时是**静默回退**到
`E:\FD2`，不报错（曾让对照实验跑错目录，见 `PROGRESS.md` §8-32）。

---

## 4. 硬约束（碰了必炸，改代码前先对照）

- **地址空间布局不可随意改**（`PROGRESS.md` §4.1）：游戏对象占 `0x10000..0x6FFFF`、
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
| 抓画面证据 | `--screenshot` + `--shot-frame`，BMP→PNG 用 `[System.Drawing.Image]::FromFile(...).Save(...)` |
| 无人值守菜单路径 | `--autokey=...`（例：`5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN` 走 continue） |
| 平台层还缺什么 | ida MCP 对照静态清单 `re/int21_ah_used.txt`、`re/int_sites_all.txt` vs `src/dos.c` 的 `switch (ah)`；日志会打印前 40 条 `UNHANDLED INT21` |
| 手工复现 fresh install | 数据文件拷到任意目录 + **删 `FD2.TMP`** → `--gamedir <该目录> --autokey=...` |
| 反汇编/反编译游戏函数 | ida MCP（主）；Ghidra HTTP 桥 `/read_memory`、`/list_segments`（批量） |

## 6. 常见坑（速查，完整清单见 `PROGRESS.md` §8，**动手前先通读 §8**）

- 命令行参数写法不匹配 = 静默用默认值（§8-32）；特权指令模拟要返回**指令长度**（§8-13）；
  调色板 6 位 DAC + DIB 是 BGRA（§8-17）；DOS 的"写 0 字节 = 截断"在 Windows 是空操作（§8-31）。
- `.gitignore` 两个陷阱（§13.5）：`*.dll`/`*.lib`/`x86/` 会挡住 vendored 三方库（需 `!vendor/**`）；
  `build/object*.bin`（letest 参考镜像）被 `*.bin` 忽略——新克隆需按 §2 方法从 Ghidra 重新导出。
- 计划状态会过期：`PROGRESS.md` §7 的勾选项以正文实测为准；发现文档与代码不符，**当场修文档**。

## 7. 文档地图

| 文档 | 内容 |
|---|---|
| `README.md` | 目标、目录、构建/运行、已验证事实、下一步、调试手法 |
| `PROGRESS.md` | 交接文档：§2 环境与命令、§3 二进制事实、§4 宿主设计与服务语义、§6 历史卡点、§7 计划、**§8 踩坑清单（必读）**、§9 调试手册、§10 ida 环境、§11 声音、§12 文件服务、§13 显示/跨平台决策 |
| `re/RE_MAP.md` | 逆向测绘地图：函数分区、AIL 边界、核心函数档案、转译路线 |
| `re/funcmap.csv` | 全量函数表（1359 行） |
| `re/*.txt` / `re/*.c` | 静态扫描清单与关键函数反编译存档 |
| 外部：github.com/wicanr2/fd2_re（`docs/`、`docs/knowledge-base/`） | 反编译踩坑与知识库（同游戏逆向资料） |
