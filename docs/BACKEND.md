# 显示与跨平台后端决策（sokol）
为什么要换掉 GDI、SDL2/SDL3/sokol 三方实测数据、git 策略、抽取 render/host/入口层的实施顺序。
对应旧 `PROGRESS.md` §13。
---

## 13. 显示/跨平台后端决策：**sokol**（0 DLL，各平台原生）（2026-10-05）

**需求重定义**（用户澄清）：要的**不是 D3D11**，只是**替换掉默认的 GDI 渲染**；
因此选 **sokol**（`sokol_app`+`sokol_gfx`+`sokol_audio`，单头文件、**0 DLL**，
Win=D3D11 / mac=Metal / Linux=GL）。SDL2/SDL3 的实测降为**备选记录**（§13.2、§13.3）。
**不用 git 分支分平台**，改用“单代码库 + 后端选择”。备选方案与实测数据见下。

> **2026-10-05 清理**：定案 sokol 后，`build/` 下的 SDL2/SDL3 全部产物已删除
> （`sdlprobe*`/`sdl3probe*` 源码与 exe、`SDL2.dll`/`SDL3.dll`、`SDL3-devel.zip`、`build/sdl3/` 解压目录、
> 仅服务于 SDL 依赖查看的 `deps.bat`）。§13.2/§13.3 的实测数据**留档**；如需重测按当时流程重写 probe。

### 13.1 sokol 实测（2026-10-05，`build/sokolprobe.c` + `build/sokol/*.h`）

probe = `sokol_app`（`SOKOL_WIN32_FORCE_MAIN`，建 960×600 窗口）+ `sokol_gfx`（D3D11）
+ `sokol_glue` + `sokol_time`，**32 位**、`/std:c11`，跑 180 帧自退。

| 项 | 实测值 |
|---|---|
| 32 位能否编/跑 | ✅ 一次通过（`sokolprobe.exe`） |
| 实际后端 | **`sg_query_backend() = D3D11`**（sokol_app 内部就是 Win32 窗口） |
| 帧耗时 | 180 帧 avg **6.174 ms/帧 = 162 fps**，`swap_interval=1`（本机高刷，未被 60 Hz 卡住） |
| **exe 体积增量** | 基线 `baseline.exe` = 116,736 B → `sokolprobe.exe` = **266,240 B，即 +149,504 B（+146 KB）** |
| **交付依赖** | `dumpbin /dependents`：**d3d11.dll、USER32、GDI32、SHELL32、KERNEL32** —— **全是系统 DLL ⇒ 交付 0 额外 DLL** |
| `d3dcompiler_47.dll` | 仅“用 HLSL 源码建 shader”时**运行时按需 LoadLibrary**（不进导入表；Win8+ 系统自带） |
| vendor 进仓库的头文件 | app 603 KB + gfx 1315 KB + audio 103 KB + time 11 KB + log 12 KB + glue 8 KB ≈ **2.05 MB 源码**（zlib 许可） |

**三方总交付体积对比**：

| 方案 | exe | 额外 DLL | **合计** | 后端 |
|---|---|---|---|---|
| **sokol（已选）** | 现 59 KB + ~146 KB ≈ **210 KB**（纹理/shader 未算，估再 +10~30 KB） | **0** | **≈ 0.2 MB** | Win **D3D11** / mac **Metal** / Linux **GL** |
| SDL2 | ~60 KB | 1.28 MB | ≈ 1.34 MB | 默认 **D3D9**（需 pin） |
| SDL3 | ~61 KB | 2.25 MB | ≈ 2.31 MB | 默认 **D3D11** |

**sokol 的真实代价（接手前必读）**：

1. **没有默认 shader**：纹理替换 GDI 必须自写一个 textured-quad shader。
   D3D11 可直接喂 **HLSL 源码**（`sg_shader_desc.attrs[i].hlsl_sem_name/_index` 指定语义，
   默认 target `vs_4_0`/`ps_4_0`，运行时 D3DCompile）；**GL 后端只能喂 GLSL 源码**
   ⇒ 跨平台就要 HLSL+GLSL 两份（macOS MSL 再一份），或用 **sokol-shdc**（GLSL 一次 → 各后端 + 生成绑定元数据）。
   本项目 shader 极简（一个四边形，~15 行/后端）⇒ **先手写 HLSL+GLSL、不引 shdc 工具链**，
   保持“代码最简”；需要 MSL 或复杂效果时再上 shdc。
2. **sokol_app 就是入口 + 主循环**：`sokol_main()` 返回 `sapp_desc`，帧由回调驱动 ⇒ `host.c` 的 `main`
   要拆成 `host_init/host_frame/host_event/host_shutdown` + 两个入口
   （`main_win32.c`：现有消息泵 + GDI；`main_sokol.c`：sokol 回调）。约 100~200 行，
   **游戏线程 / watchdog / LE·DOS 层不动** —— 这就是 §13.6 第 1 步。
3. **键码映射**：`SAPP_KEYCODE_*` → BIOS 扫描码需自建表（~60 行；现在 Win32 用 `MapVirtualKeyA`）。
4. **音频**：`sokol_audio`（WASAPI/CoreAudio/ALSA·Pulse）回调替换 waveOut ⇒ 顺带实现低延迟流式。
5. **Linux 构建**：X11 需 `libX11-dev`，Wayland 可选；比 SDL2 的“系统包”稍麻烦。
6. **API 仍在演进**：本次 probe 用到的已是 `sg_view`/`sg_sampler`/`sg_environment` 新一代 API，
   **网上大量 sokol 教程已过时** —— 一律以 vendor 进仓库的头文件内文档为准（这也是要 pin 版本的原因）。

### 13.2 SDL2 实测（probe 已删，数据留档；原 `build/sdlprobe*.c`，SDL2 2.32.8 **x86**，宿主是 32 位进程）

| 项 | 实测值 |
|---|---|
| `SDL2.dll`（x86） | **1,338,880 B ≈ 1.28 MB**（x64 是 1,576,448 B） |
| Windows 可用渲染后端 | `direct3d`、**`direct3d11`**、`direct3d12`、`opengl`、`opengles2`、`software` |
| **SDL2 默认加速后端** | **`direct3d` = D3D9，不是 D3D11** ⇒ 必须显式指定，否则“用了 SDL 就没用上 D3D11” |
| `SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11")` | ✅ **一行生效**：日志 `renderer in use: direct3d11` |
| W3 每帧 CPU 成本 | `UpdateTexture(320×200 ARGB)` = 0.136 ms；`+Clear+Copy(→960×600)` = **0.092 ms**；`+Present` = 0.086 ms（3000 次取均值） |
| 加 `SDL_RENDERER_PRESENTVSYNC` | 6.24 ms/帧（vsync 等待；隐藏窗口下不是满刷新率，仅供量级参考） |
| 32 位链链 | ✅ `lib/x86/SDL2.lib` 存在，probe 编译/运行均通过（`sdlprobe*.exe`，已随清理删除） |

⇒ W3 的渲染路径 CPU 成本约 **0.1 ms/帧**，而且 GPU 负责缩放（GDI 是软件缩放）。
另：帧率卡在 32 fps 是 `SetTimer(20)` 撞上 Windows 默认 15.625 ms 定时器粒度（§12/§7.1），
改 SDL 事件循环 + `PRESENTVSYNC` 顺带解决。

### 13.3 SDL3 实测对照（2026-10-05，同流程同 probe）

**问题**：SDL3 是否也能像 SDL2 那样“产物只多 1 个 DLL”？→ **是**，但 DLL 更大。

| 项 | SDL2 2.32.8（x86） | **SDL3 3.4.18（x86）** |
|---|---|---|
| 交付物 | `SDL2.dll` **1,338,880 B = 1.28 MB** | `SDL3.dll` **2,361,344 B = 2.25 MB**（**+76%**） |
| DLL 自身依赖 | 仅 Windows 系统 DLL（`dumpbin /dependents`） | **同样仅系统 DLL**（SETUPAPI/WINMM/IMM32/VERSION/KERNEL32/USER32/GDI32/…，一模一样） |
| exe 增量 | `sdlprobe.exe` = 117,760 B | `sdl3probe.exe` = 118,784 B（**+1 KB**，可忽略） |
| **默认渲染器** | **`direct3d`（D3D9）** ⚠️ | **`direct3d11`** ✅ 不用 pin |
| 后端清单 | direct3d、direct3d11、direct3d12、opengl、opengles2、software | direct3d11、direct3d12、direct3d、opengl、opengles2、**vulkan**、**gpu**、software |
| 显式 pin D3D11 | `SDL_SetHint(SDL_HINT_RENDER_DRIVER,"direct3d11")` ✅ | 同一 hint ✅（或 `SDL_CreateRenderer(win,"direct3d11")`） |
| 每帧 CPU 成本 | Update 0.136 / +Clear+Copy **0.092** / +Present 0.086 ms | Update 0.108 / +Clear+Texture **0.081** / +Present 0.126 ms —— **同一量级，无差别** |
| x86 导入库 | ✅ | ✅（另有 x64/arm64） |
| 需要 SDL2main？ | 需（我们用 `SDL_MAIN_HANDLED` 绕开） | 不需要 |
| **API 兼容性** | — | **与 SDL2 源码不兼容**：`SDL_RenderCopy`→`SDL_RenderTexture`、`SDL_CreateRenderer(win,name)`、`SDL_CreateWindow(title,w,h,flags)`、`SDL_GetVersion()` 返 int、`SDL_GetTicks()` 返 Uint64、事件改 `SDL_EVENT_*`、**音频旧 API 整个换成 `SDL_OpenAudioDeviceStream` + `SDL_AudioStream`** |

**结论与选择依据**：

- 交付模型**两者相同**：一个自带依赖的 DLL，exe 本身几乎不变（+1 KB）。
- SDL3 的加分：**默认就是 D3D11**（SDL2 默认 D3D9，必须 pin）、主线维护（SDL2 已进维护模式）、
  Linux 上 Wayland 一等公民（第 4 步受益）、多出 `vulkan`/`gpu` 后端。
- SDL3 的代价：DLL **大 0.97 MB**；**音频 API 重写**；Windows 上资料相对少。
- **已定 sokol（§13.1）**，SDL2/SDL3 降为备选：两者仍保留为“若 sokol 卡壳时的回退路线”，
  且本两份实测证明了“换库只动 `window/render/audio` 三个文件”（SDL2 probe → SDL3 probe 重写约半小时）。

### 13.4 备选“更小巧”的跨平台库（选型记录）

| 方案 | 交付体积 | 代码量 | 后端 | 判断 |
|---|---|---|---|---|
| **SDL2（W3，已选）** | DLL **1.28 MB**（实测） | **最少**：渲染 ~70 行 + 窗口/输入 ~150 行 + 音频 ~80 行 | Win 默认 D3D9，**一行钉到 D3D11**；mac/Linux 走 GL | ✅ 选它 |
| sokol（app+gfx+audio 单头三件套） | **0 DLL**，exe **+146 KB（实测）** | 中：**每个后端一份 shader**（HLSL/GLSL/MSL）或 sokol-shdc | Win **D3D11 原生**、mac **Metal 原生**、Linux GL | ✅ **已选（§13.1）** |
| GLFW + OpenGL + miniaudio | 0 DLL，exe +150~300 KB（估） | 中 | 只有 GL（mac 最高 4.1 且已废弃） | 拿不到 D3D11/Metal，放弃 |
| raylib（静态） | 0 DLL，exe +300~600 KB（估） | 少，但自带一整套游戏框架 | Win/mac/Linux/Web | 对“移植宿主”是多余抽象，体积反而最大 |
| SDL2 **静态链接** | 0 DLL，**exe 反而 +0.6~0.9 MB** | 同 W3 | 同 W3 | 想要“单文件无 DLL”时的反直觉结果：比带 DLL 更胖 |
| 纯 Win32 + D3D11（W1） | 0 | 多 ~300–400 行窗口/输入 | 仅 Windows | 体积最小但代码不是最少，与优先级矛盾 |

**两个关键认知**：
1. **Windows 上 SDL2 默认是 D3D9**，D3D11 必须显式 pin（实测一行即可）；**SDL3 默认就是 D3D11**
   （见 §13.2）——W3 与“用系统 D3D11”不冲突，选 SDL3 则连 pin 都不必。
2. **分发体积的痛点只在 Windows**：Linux 上 SDL2 是系统包（装机一行命令），macOS 可静态；
   为了省 1.28 MB 的 DLL 去换掉整套简单代码不划算。真要 0 DLL，路径是**加 `render_sokol.c`**，
   而不是开 git 分支。

### 13.5 git 策略与两个 .gitignore 陷阱

- 仓库根 = `E:\FD2\port`（单提交 `aeb744e`，40 文件，工作区干净）。
- **不用分支分平台**：本项目正处高频修 bug 阶段，分支会把“一个 fix 修 N 遍 + 回归 N 次”
  放大，并分裂最值钱的逆向文档。约定：`main` 单线，`platform/*` 只做**短命**集成分支（合并即删）；
  **真正需要长期分支的时机**只有架构级分叉（ARM 走源码化/模拟器、`release/` 冻结）。
- ⚠️ `.gitignore` 的 `*.dll`、`*.lib`、`x86/`、`x64/` 会**挡住 vendored SDL2**：
  把 SDL2 放进 `port/vendor/` 时必须加 `!vendor/**` 例外（否则换机器/新克隆编不过）。
- ⚠️ `build/object*.bin`（Ghidra 参考镜像）被 `*.bin` 忽略 ⇒ 新克隆无法跑 `letest.exe`，
  需加白名单或按 §2 的 Ghidra HTTP 桥方法重新导出。

### 13.6 实施顺序（每步一提交 + `regress.ps1` 回归）

1. ✅ **抽接口 + 拆入口**（已完成 2026-10-05，纯重构、行为不变）：
   - 新增 `render.h` + `render_gdi.c`：`blit()` 的**送显部分**逐字节搬入（`StretchDIBits`、
     `BITMAPINFO`、固定整数缩放全不变）；调色板→BGRA 转换与 `--screenshot` **留在共享层**，
     保证任何后端都拿到同一份像素（对拍基准不随后端走）。
   - 新增 `host.h`：`host_init/host_render_desc/host_start/host_frame/host_key/
     host_wants_frames/host_request_quit/host_shutdown` + 入口层必须提供的 `input_post_vk()`。
   - 新增 `main_win32.c`：`fd2_entry`、窗口/消息泵/定时器、Win32→BIOS 键盘翻译、
     `input_post_vk`（`--autokey` 靠它注入按键）。
   - vendor：sokol 头文件进 `port/vendor/sokol/`（pin commit `2e75443`，含 README：许可、
     升级步骤、实测体积）。
   - `build.ps1` 加 `-Render gdi|sokol`；**未实现的后端会明确报错**，不会静默回落。
   **验收**：`regress.ps1` **8/8 PASS**；日志 `host: render backend = gdi`；45 s **1440 帧 =
   恰好 32.0 fps**（再次印证 §13.1 的定时器量化结论）；`build/regress.bmp`（帧 900）与重构前画面一致。
2. **`render_sokol.c`**：sokol_app 窗口/事件 + `sg_make_image`（320×200 RGBA8, `dynamic_update`）
   每帧 `sg_update_image` + **手写 HLSL textured quad** + `swap_interval=1`（顺带解掉 32 fps）；
   键码 `SAPP_KEYCODE_*` → BIOS 扫描码表（~60 行）。`--render=sokol` 默认，`gdi` 保底对拍。
   **验收**：GDI vs sokol 同帧截图逐像素一致。
3. ✅ **`audio_sokol.c` + 抽出 `audio.h`**（完成 2026-10-06，§41）：sokol_audio（WASAPI）
   pull 回调做软件混音，`ail.c`/`synth.c` 只见 `audio.h` 接口，音乐与音效**共用一个设备**；
   “音量在 synth 后端不生效 / 渐变未实现”早已在 §11.8/§11.9 修掉，本轮是**换承载点**、
   并补上可测量的音频判据 `--audio-dump`。详见 `docs/AUDIO.md` §11.10、`docs/rounds/11-audio-mixer.md`。
   **验收**：设备每进程只开 1 次；`audio: mixed …` 每 10 s 单调前进；music/sfx 分声道峰值均 > 0；
   `--audio-dump` 逐秒 RMS 连续、`--volume` 10→100 实测 10.3×；`ail: play 16`/`(cut) 0`；回归 8/8。
4. ✅ **POSIX + `platform.h`（第 1 刀已完成，§43）**：`platform.h` + `platform_win32.c` +
   `platform_posix.c` 已抽出**内存**这一层，`le.c` 零 Win32 依赖，`letest` 在
   Windows/Linux **三个对象哈希完全一致**（`Makefile.linux`）。剩下的三刀与门槛
   （`dos.c` 的 VEH/文件服务、入口层键码与截图、**`-m32` 才能执行 32 位游戏代码**）
   见 `docs/rounds/13-portability.md` §43.5。原计划：抽 OS 适配层（内存/线程/文件/异常），Win32 实现进
   `platform_win32.c`（VEH 暂时仍留 `dos.c`，语义转换见下），POSIX 实现进 `platform_posix.c`
   （`sigaction`/`mmap`/`pthread`）；Linux 侧需要 `libX11-dev` + sokol GL 后端。
   `int NN`/`in out` 的信号语义用 `probe4.c` 的方法在目标机重测 → 首个非 Windows 产物。
   **第 2 刀已落（2026-10-07，§45）**：`dos.c` 过河 —— `dos_fault.h` 把“故障怎么来”（VEH /
   sigaction 两个薄包装）与“故障是什么”（可移植 `dos_fault_core`）切开；`dos.h` 去 `windows.h`，
   引入便携 `dos_ctx` 与自检入口 `dos_service`；platform.h 第 2 切片（文件 `pread/pwrite`
   由调用方持位置、线程/时间/进程/`plat_readable`）。**信号语义已按计划在目标机实测**
   （freestanding `-m32` 探针 `src/faultprobe32.c`，不需要 gcc-multilib）：`int NN`/特权指令/
   段错误在 i386 compat 下**同为 `SIGSEGV SI_KERNEL` 且无 `si_addr`** ⇒ 靠解码 EIP 字节区分
   （`docs/PITFALLS.md` §8-61、`docs/rounds/15-dos-and-faults.md` §45.2）。判据：新工具
   **`doscheck`** 两平台同一套 **49/49**（含真 `int 0x21` 经故障入口分发 + CF 回写）、
   `letest` 三哈希仍逐字相同、回归 8/8、跨版本同 tick A/B **0 px**。

### 13.7 sokol 后端实测（2026-10-06，`-Render sokol` 真跑了一次）

第 2 步的代码其实**已经写完并且能编能跑**（`src/main_sokol.c` 264 行 / `src/render_sokol.c`
286 行 / `src/sokol_impl.c`），只是**验收没做**、**默认还是 gdi**（已改，见 §13.9）。本次实测（同一沙箱
`build/sandbox33`、`--gamedir=build/sandbox33 --exit-after=6`）：

| 项 | `-Render gdi` | `-Render sokol` |
|---|---|---|
| 日志 | `host: render backend = gdi` | `host: render backend = sokol` |
| 图形栈 | StretchDIBits | `sokol: backend=D3D11 image=320x200 … swapchain=960x600` |
| **6 s 帧数** | **195 帧（32.5 fps）** | **928 帧（≈155 fps）** |
| exe 体积 | **95,232 B** | **286,720 B（+191,488 B）** |
| 窗口标题 | `…native host (POC)` → 现为 `(gdi)` | `…native host (sokol)` |

**两条结论（对第 2 步验收有直接影响）**：

1. **`swap_interval=1` 在无显示/RDP 会话里不生效**，sokol 退化成不限速（928 帧 vs 195 帧，
   **4.8×**）。所以 §13.6 第 2 步那句"**同帧**截图逐像素一致"**根本无法执行**——两个后端
   在同一墙钟时间内跑的游戏帧数不同，`--shot-frame=700` 取到的游戏状态不是同一个。
2. **"POC" 只是窗口标题里的字样**（原 `main_win32.c:167` `FlameDragon2 - native host (POC)`，
   2026-10-06 起改成 `(gdi)`），它指的就是 §13.6 第 1 步抽出来的 **GDI 参考实现入口层**，
   不是另一条技术路线。GDI 之所以一直当调试/回归基准：`build.ps1 -Render` 默认 `gdi`、
   帧节奏确定、全进程内单线程便于断点、且第 2 步验收尚未落地。
   （**已过时**：§13.9 已把默认后端改成 sokol、§13.10 已完成验收 —— GDI 现在只在需要
   “参考实现”时按需 `-Render gdi` 重建，不进日常构建/回归/抓帧循环。）

### 13.8 验收标准改判：按帧号 → 按时间 → 按 guest tick（2026-10-06）

§13.6 第 2 步那句"同帧截图逐像素一致"是**错误的标准**，已改判。理由与实测：

**为什么帧号不行**：帧号只有在同一后端、同一帧率下才是一个"时刻"。32 fps 与 158 fps 的第 700
帧相差 20 秒的游戏时间。

**为什么时间也不够**：`--shot-time=<ms>`（新增）按墙钟触发，两个后端确实落在同一毫秒量级：

```
host: frame  508 dumped … (age 16016 ms)     -Render gdi    31.8 fps, 640 帧/20 s
host: frame 2525 dumped … (age 16000 ms)     -Render sokol 158.3 fps, 3188 帧/20 s
```

但**帧周期 = 采样误差**：GDI 帧周期 31 ms，触发条件"age >= 16000"最多会**迟到一整个帧周期**，
实测迟到 16 ms；sokol 帧周期 6.3 ms，只迟到 0 ms。于是 GDI 的取样比 sokol **多走了一步动画**
（画面逐像素差 10.29%，差异区域全在动画精灵/文字上，肉眼就是"差 1 帧"）。
**这正好说明高帧率按时间采样更精确**——32 fps 的采样精度是 ±31 ms，158 fps 是 ±6 ms。

**所以真正的标准是 `--shot-tick=<n>`**：直接用**游戏自己的时钟**（BIOS tick `0x40:0x6C`，
由 `dos.c bios_tick_thread` 以 18.2 Hz 独立推进，与帧率无关）触发抓帧。同一个 tick 计数 =
同一个 guest 状态，与后端帧率完全无关。日志会同时打出 `age`/`guest tick`，采样误差可见。

| 触发 | 精度 | 适用 |
|---|---|---|
| `--shot-frame=<n>` | 只在固定帧率下有意义 | 既有脚本（`regress.ps1`、`framediff.ps1` A/B） |
| `--shot-time=<ms>` | ±1 个帧周期（GDI ±31 ms / sokol ±6 ms） | 墙钟对齐、人类可读 |
| **`--shot-tick=<n>`** | **±0（同一 guest 状态）** | **跨后端对拍（推荐）** |

⚠ 抓帧路径用**绝对路径**：宿主会 `chdir` 到游戏目录，相对路径会静默写不出 BMP
（日志 `host: cannot write frame dump …`）。

结论：改用 tick 触发后，"**高帧率不可比**"这个理由**不成立**——sokol 反而更适合做测量平台
（采样 ±6 ms vs GDI ±31 ms，且能跑到 158 fps）。第 2 步的验收应改成
"**同一 guest tick 下的截图逐像素一致**"。GDI 的定位相应从"默认"降为**对拍/断点基准**，
默认后端改判见 §13.9。

### 13.9 默认后端改判：gdi → sokol（2026-10-06）

**问题**：`build.ps1 -Render` 默认 `gdi`，且两个后端**输出到同一个 `build/fd2host.exe`**，
后编的覆盖先编的。于是"默认启动"就是最后一次构建用的后端——一直是 GDI，窗口标题
`…native host (POC)`，看起来像项目卡在 POC 阶段。

**为什么没有运行时 `--render=` 开关**：`-Render` 只能是**构建期**开关。两个入口层都定义
`int main(int argc, char **argv)`（`main_win32.c:179`、`main_sokol.c:242`，由 `entry.c` 的
`fd2_entry → mainCRTStartup` 调用），且 sokol_app **独占窗口与帧循环**（回调驱动）而 GDI 用
Win32 消息泵 + `SetTimer`，二者不能链进同一个二进制。要做运行时切换得先把两边改名为
`host_main_gdi()` / `host_main_sokol()` 再在 `entry.c` 里分发——目前没这个需求。

**改动**：

| 项 | 改前 | 改后 |
|---|---|---|
| `build.ps1` 默认 | `[string]$Render = "gdi"` | `[string]$Render = "sokol"` |
| GDI 窗口标题 | `…native host (POC)` | `…native host (gdi)`（`main_win32.c:167`） |

"POC" 字样从此只出现在 `host.c` 的启动横幅与里程碑描述里（指项目阶段，不指后端）。

**验证**：`aux_build.bat fd2host`（不带 `-Render`）→ 产物 287,232 B，日志
`sokol: backend=D3D11 …` + `host: render backend = sokol`，5 s **788 帧 / 154.7 fps**。

**回归（关键：证明换后端不改游戏行为）**：同一套 `regress.ps1` 参数在 sokol 下
**8/8 PASS**、`FD2.TMP = 207360` 与原件一致、`ail: play 17` / `(cut) 0`，与 GDI 跑出的
数字**逐项相同**——游戏节奏由 guest BIOS tick（18.2 Hz 独立线程）驱动，不受后端帧率影响
（sokol 14 s / 2361 帧走完同一段流程）。这也说明 §13.6 第 2 步的验收只需比像素，不必比帧数。

**想回 GDI**：`aux_build.bat fd2host -Render gdi`。注意它写的是同一个 `fd2host.exe`；
需要两份并存时自行 `cp build/fd2host.exe build/fd2host_gdi.exe`。

### 13.10 第 2 步验收落地：sokol 与 GDI 同 tick 逐像素比（2026-10-06，§36）

按 §13.8 定的"同一 guest tick 逐像素"标准实测，**sokol 通过**。全流程：

| 步骤 | 做法 | 结果 |
|---|---|---|
| 基线（同后端自比） | 默认构建（sokol）连跑两次，每次先删 `FD2.TMP`，`--autokey=<标准配方> --shot-tick=600` | **0 / 64000 px（0.0000%）** |
| 跨后端 | `aux_build.bat fd2host -Render gdi` 出对照图（同参数、同 tick） | **31 / 64000 px（0.0484%）**，容差 0.5% 内 |
| 差异定位 | 逐像素扫描 bbox | 全部落在 **x[40..53] y[141..144]** 一块 14×4 小区（一个动画元素的相位差），**不是**调色板/通道序/缩放这类系统性差异 |
| 状态一致性 | 日志 | 同 tick 下 `age 37235/37282 ms`、帧数 `5937`(sokol) vs `1185`(GDI) ⇒ **状态由 tick 钉住，与帧率无关** |
| 回归 | 切回默认 sokol 构建后 `regress.ps1` | **8/8 PASS**、`FD2.TMP = 207360` |

**取样点必须是"静止画面"**（关键修正）：先按老办法抓 tick 290（片头转场中），
**同后端自比都差 60%**（一张黑场转场帧、一张已入戏的场景帧）—— 那一段有时间轴动画，
启动到进入该场景的耗时在两次运行之间不同，同 tick ≠ 同状态。
判据与正确做法见 `PITFALLS.md` §8-53。**验收取样点定为 tick 600**
（标准 autokey 走完、停在静态等键画面），此时基线为 0，跨后端差异才有意义。

**GDI 的位置**：只为这一次对照临时 `-Render gdi` 编过一回，之后立即切回默认 sokol；
日常循环（构建/回归/抓帧）不再涉及 GDI，它只在需要"参考实现"时按需重建。

### 13.11 Linux 窗口系统：**X11（经 XWayland）** —— Wayland 原生不在 sokol 里（2026-10-07，第 46 轮）

**问题**（用户提问）：描述里写的是 X11，Wayland 到底支不支持？X11 能不能在 Wayland 里用？

**结论**（三句话）：

1. **pin 住的 sokol 没有 Wayland 后端**：`vendor/sokol/sokol_app.h`（commit `2e75443`）在
   `_SAPP_LINUX` 下只 `#include <X11/Xlib.h>`、`XInput2`、`Xcursor`，全文件
   **0 处** `wayland` / `wl_display` / `wl_surface` ⇒ 我们的 Linux 宿主是**纯 X11 客户端**。
2. **X11 客户端在 Wayland 桌面照常跑**，因为桌面会起 **XWayland**（X 服务器兼容层，集成在
   GNOME/KDE/Sway 等会话里；**WSLg 也是**）。代价只有缩放/延迟这类边缘问题，功能不受影响；
   只有**没装 XWayland 的极简 Wayland 会话**（裸 sway 之类）才起不来。
3. **要 Wayland 原生就得换后端**（SDL3 是 Wayland 一等公民，见 §13.3）——本轮**不做**：
   sokol 的 X11 后端已经把我们要的东西（布局无关键码 + `XLookupString` 的字符）都做好了
   （见下），换库的收益不抵成本。**记录为"accepted limitation"**。

**实测证据**（本机 WSLg，探针 `build/x11probe.c`，命令 `cc -std=gnu11 -o build/x11probe
build/x11probe.c -lX11 -lGL && ./build/x11probe`；探针是 build/ 下的临时产物、不入库）：

```
server vendor   : The X.Org Foundation
protocol version: 11.0
screens         : 1  default 1920x1080 depth 24
extensions      : XWAYLAND=yes GLX=yes XInput=yes XKEYBOARD=yes
=> served by XWayland (an X11 client under a Wayland compositor)
GLX version     : 1.4
GLX RGBA/dbuf   : yes (a visual is available)
```

⇒ `DISPLAY=:0` 走 `/tmp/.X11-unix/X0` 连到 **WSLg 的 XWayland**（`XWAYLAND` 扩展在 = 这是
XWayland 而不是真 Xorg）；`SOKOL_GLCORE` 要的 GLX 1.4 + RGBA 双缓冲 visual 都在。

**对入口层的直接影响：不用自己写 X11 代码**（这是本次探查最大的收获）。sokol_app 的 X11 后端已经：

| 我们要的 | sokol_app 给什么 | 代码位置 |
|---|---|---|
| 布局无关的物理键 | `sapp_event.key_code`（`SAPP_KEYCODE_*`，XKB key name 表生成，GLFW 同法） | `_sapp_x11_init_keytable()` |
| ASCII/字符 | **`SAPP_EVENTTYPE_CHAR`**（内部就是 `XLookupString` + keysym→unicode 表） | `_sapp_x11_handle_keypress()` |
| 修饰键 | `sapp_event.modifiers`（`SAPP_MODIFIER_*`） | `_sapp_x11_mods()` |

所以 Linux 入口层要写的只有**一张 `SAPP_KEYCODE_*` → BIOS 扫描码表**（外加 `0xE0` 规则），
**不是** `XLookupString`/keysym 表——`docs/rounds/13-portability.md` §43.5 第 3 项的原计划
（"`ToAscii` 的 ASCII 生成 → X11 `XLookupString`/keysym 表"）据此**收敛**为：
"`SAPP_KEYCODE` → 便携键 id → BIOS 扫描码；字符用 `SAPP_EVENTTYPE_CHAR`"。

**依赖**：X11 侧编译/链接已具备（`libx11-dev libxcursor-dev libxi-dev libgl1-mesa-dev`，
`-std=gnu11 -DSOKOL_GLCORE`，见 §13.6 第 4 步 / `rounds/13` §43.6）。
