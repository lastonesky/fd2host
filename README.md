# FD2（炎龙骑士团 2 · Flame Dragon 2 黄金城之谜）→ Windows 原生移植工程

路线 C：**32 位二进制宿主 + 逐步源码化**。把 `E:\FD2\FD2.EXE`（DOS/4GW 32 位保护模式游戏，
Watcom + Miles AIL）在 Windows 上**原生**跑起来 —— 不模拟 DOS、不模拟实模式、不用 DOSBox。
图形/声音/输入换成现代 Windows 实现，游戏逻辑先保留原始 x86 机器码，再逐函数换成还原的 C 源码。

**当前里程碑：POC 已达成** —— 画面 + 声音 + 能进剧情，且正在把机器码逐模块换成 C（已接入 50 个）。

---

## 目录

```
port/
├── README.md        ← 本文件：项目卡片 + 快速开始（细节一律去 docs/）
├── PROGRESS.md      ← 进度时间线 + 下一步计划（轮次明细在 docs/rounds/）
├── AGENTS.md        ← 协作约定：工作流、硬约束、常用命令
├── docs/            ← 分类文档（见下方"文档地图"）
├── build.ps1        构建脚本（vcvars32 + cl，32 位目标）
├── regress.ps1      一键回归：重建沙箱 → autokey 走 continue → 8 项断言
├── framediff.ps1    两个抓帧 BMP 的逐像素差分（repl A/B 证据）
├── src/
│   ├── le.c/.h      LE 加载器：对象/页/fixup 重定位、地址空间预留
│   ├── dos.c/.h     平台层：VEH、int/端口接管、DOS/DPMI/BIOS 服务、低内存镜像
│   ├── render.h + render_gdi.c   渲染后端接口 + GDI 实现（对拍基准）；render_sokol.c = 默认后端
│   ├── host.h/.c    内核：参数、LE/DOS/AIL 启动、游戏线程、调色板→BGRA、抓帧、watchdog/autokey
│   ├── main_win32.c 入口层：fd2_entry、窗口/消息泵、Win32→BIOS 键盘
│   ├── repl.c/.h    源码接入层：已对拍的转译函数入口改成 5 字节 jmp 指向 C（默认全开）
│   ├── ail.c + xmidi.c + synth.c + dls.c   AIL 替换层 / XMIDI / 软件合成器 / gm.dls 音色
│   ├── letest.c     加载器自检（对拍 Ghidra 镜像）
│   ├── probe*.c     可行性探针
│   ├── game/        源码转译产物（详见 docs/TRANSLATION.md）
│   └── *check.c     各模块的机器码对拍测试
├── re/              逆向工作台：RE_MAP.md、FDPS_MAP.md、funcmap.csv、静态扫描清单、preflight.py
└── build/           输出：fd2host.exe、host.log、*.bmp 证据（不入 git）
```

## 快速开始

```powershell
# 构建（MSVC 14.51 / 32 位目标）
pwsh -File E:\FD2\port\build.ps1 -Target fd2host

# 运行：宿主是 WINDOWS 子系统，**日志恒写 port/build/host.log**
Start-Process E:\FD2\port\build\fd2host.exe -ArgumentList '--exit-after=25' -WorkingDirectory 'E:\FD2'

# 加载正确性（唯一可信判据）：与 Ghidra 重定位镜像逐字节对比
& E:\FD2\port\build\letest.exe

# 一键回归（8/8 PASS 为准）
pwsh -File E:\FD2\port\regress.ps1
```

⚠ **先看日志里的 `host: working directory = …`**：命令行参数没被识别时会**静默回退**到 `E:\FD2`，
不报错（曾让对照实验跑错目录）。全部参数与含义见 **`docs/ENVIRONMENT.md`**。

沙箱里若 `vcvars` 起不了 `reg.exe`（docs/rounds/05-rec-and-services.md §33.5），走：
`cmd //c E:\FD2\port\aux_build.bat fd2host`。

## 已验证的关键结论（一句话版）

| 主题 | 结论 | 详情 |
|---|---|---|
| LE 加载 | 7937 条 fixup，obj1/obj2 与 Ghidra 镜像**逐字节一致**；跨页 fixup 必须跳过 | `docs/BINARY-FACTS.md` |
| 地址空间 | 游戏对象占 `0x10000..0x6FFFF`，低内存镜像 `0x70000..0x7FFFF`，VGA `0xA0000`；低 64 KiB 不可映射 | `docs/HOST-DESIGN.md` §4.1 |
| 宿主映像 | 必须小（~288 KB）**且保留 ASLR**（`/BASE:0x60000000`）；大静态数组会压掉游戏窗口 | `docs/HOST-DESIGN.md` §4.1 |
| 服务语义 | `AH=42` 是 CX:DX 入参 / DX:AX 出参、`AH=48` 返回线性地址且要读完整 EBX、`AH=FF` 必须非 0… | `docs/HOST-DESIGN.md` §4.4 |
| 显示 | VGA DAC 是 **6 位/通道**（要 `(v<<2)\|(v>>4)`）、32bpp `BI_RGB` 内存序是 **BGRA** | `docs/PITFALLS.md` §8-17 |
| 键盘 | 菜单走 `INT 16h`、片头轮询 BDA；**游戏不用鼠标**（静态 + 运行期双证） | `docs/PITFALLS.md` §8-22 / `docs/rounds/01-platform-and-tooling.md` §12.3 |
| 音频 | 16 个 AIL 入口替换；音乐自带合成器 + 解析 `gm.dls`（不依赖系统 MIDI） | `docs/AUDIO.md` |
| 转译 | 50 个函数经 `src/repl.c` 接入运行中的游戏，全部逐字节对拍通过 | `docs/TRANSLATION.md` |

> 以上每条背后都有硬判据（`host.log` 行 / `letest` 逐字节 / 抓帧 / `*check` 用例数），
> 未证实的一律标注"待确认"。**禁止**用字节扫描或"听起来像"下结论。

## 下一步（摘要，完整版见 `PROGRESS.md`）

1. **源码化继续**：下一个目标是 `sub_15F84` 脚本 VM（方法、依赖、判据见 `docs/TRANSLATION.md`）。
2. ~~显示层换 sokol~~ **已验收（§36）**：默认后端就是 `sokol`（D3D11 / 155 fps），GDI 降为
   `-Render gdi` 对拍基准（不进日常循环）。同 guest tick 下**同后端基线 0 px**、
   GDI vs sokol **31 px（0.0484%）**；取样点定为静止画面 `--shot-tick=600`（片头转场同后端
   自比都能差 60% ⇒ **先验基线再比跨后端**）。详见 `docs/BACKEND.md` §13.10。
3. **音频治本**：SFX 爆音已按"设备常驻 + 3 ms 起停斜坡"修完；背景音乐已改成**流式合成**
   （原版 AIL 的增量渲染架构）并补上 `AIL_set_sequence_volume` 的 `ms` 渐变，
   进商店/剧情切换时的淡出淡入不再丢失；**每首曲子开头 2 秒小声 = 原作自己的淡入**
   （四段证据链见 `docs/AUDIO.md` §11.9）。剩下的是软件混音
   （sokol_audio 统一音乐 + 音效），见 `docs/AUDIO.md` §11.6 / §11.8。
4. 稳定性长跑 / 首次存档路径实测；跨平台走"单代码库 + 后端选择"，不用 git 分支。

## 文档地图（`docs/`）

| 文件 | 内容 |
|---|---|
| `docs/INDEX.md` | **总导航**（含旧 `PROGRESS.md` 的 §编号 → 新文件对照表） |
| `docs/ENVIRONMENT.md` | 环境、构建/运行、**全部命令行参数**、Ghidra 桥、IDA MCP |
| `docs/BINARY-FACTS.md` | `FD2.EXE` 容器、LE 头实测偏移、fixup 格式、对象布局 |
| `docs/HOST-DESIGN.md` | 地址空间、源文件职责、VEH 四类异常、服务返回值语义 |
| `docs/PITFALLS.md` | **踩坑清单（动手前必读）** + 历史卡点 |
| `docs/DEBUG-MANUAL.md` | 诊断手段、自检/对拍工具清单、崩溃转储字段 |
| `docs/AUDIO.md` | AIL 替换层、XMIDI、合成器、gm.dls、已知杂音 bug |
| `docs/BACKEND.md` | sokol 选型实测、git 策略、跨平台抽取顺序 |
| `docs/TRANSLATION.md` | 源码转译方法 + 模块清单 + 接入机制 |
| `docs/FDPS-ARCHIVE.md` | 炎龙外传 FDPS（**已冻结**）成果存档 |
| `docs/rounds/*.md` | 逐轮病历（按主题归堆：平台/工具、叶子工具层、地基管线、对话框 UI、记录与服务） |
| `docs/knowledge-base/` `docs/data/` | 外部 `fd2_re` 参考快照（**另一个 build**，见 `docs/KEEP.md`） |
