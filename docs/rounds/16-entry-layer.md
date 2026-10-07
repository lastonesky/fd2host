# 轮次明细：跨平台第 3 刀 —— 入口层（键码/字符/截图 + host.c 的平台缝）
本轮把"窗口 + 输入 + 主循环"这半边从 Win32 抽出来，让 Linux 侧能编译、能在 -m32 下真跑。
先回答一个阻塞性问题：**Wayland 到底能不能用**（用户提问，见 `docs/BACKEND.md` §13.11）——
结论：pin 住的 sokol **只有 X11 后端**，但 X11 客户端在 Wayland 桌面经 **XWayland** 照常跑，
本机 WSLg 实测就是 XWayland；且 sokol 的 X11 后端已经替我们做了 `XLookupString` 与布局无关键码，
**我们不用自己写 X11 代码**。

---

## 46.1 本轮起点（`rounds/15` §45.7、`rounds/13` §43.5）

跨平台已完成：§43 加载器（`le.c`）、§45 DOS 层（`dos.c` + `dos_fault.h`），两平台判定都用
`letest` 三对象哈希一致 / `doscheck` 49 条断言。剩下三件事：

1. **入口层**（本刀）：
   - `MapVirtualKeyA`/`ToAscii` → 便携键表（本刀已起头，见 §46.3）；
   - 截图 `winshot.c`（`PrintWindow`）在 Linux 侧怎么办（`--screenshot` 本来就由共享层
     `host.c` 直接写 BMP，与后端无关，见 §46.2）；
   - `host.c` 里的 Win32 缝：`GetTickCount`/`Sleep`/`CreateThread`/`FindFirstFileA`/
     `GetModuleFileNameA`/`SetCurrentDirectoryA`/`_dup2`/`ExitProcess`；
   - `keylog.c`（`DWORD`/`MapVirtualKeyA`）与 `platform.h` 第 3 切片（时间/线程/目录/文件属性）。
2. **`-m32`**：游戏是 32 位 x86，64 位宿主跑不了 ⇒ `gcc-multilib` + 32 位 X11/ALSA 多架构包
   （`apt-cache policy gcc-multilib` 有 candidate，tuna 镜像可达；本轮只确认到这一步）。
3. `host.c`/AIL 里剩余的线程与时间（`GetTickCount`/`Interlocked*`）——归本刀。

## 46.2 截图路径的先验结论（`--screenshot` vs `--wshot`）

`--screenshot` 的两条触发（tick/time/frame）与 **BMP 落盘**都在 `host.c`
（`dump_frame_bmp()` 直接把 `g_rgb` 按 32bpp BI_RGB 写出），**与渲染后端无关** ⇒ Linux 上
`--screenshot` 一行不用改就能用。只有 `--wshot`（窗口内容抓取）走 `PrintWindow`
（`winshot.c`，Win32 专属），Linux 侧先 stub（返回失败并打日志），不影响对拍用的
`--screenshot`（跨后端比画面用的就是它，`docs/BACKEND.md` §13.8）。

## 46.3 第一切片：便携键表 `src/keys.h` / `keys.c`（已落）

**问题**：Windows 入口层用 `MapVirtualKeyA`（VK→BIOS 扫描码）+ `ToAscii`（VK→ASCII）；
Linux 侧两者都没有。而 `--autokey` 用**文本**点名按键、`--keylog` 把按键名写回文件，
所以两个平台需要**同一个"按键身份"**，BIOS 键盘环只能吃 `(scan, ascii)`。

**做法**（`FR_KEY_LIST` X-macro 是唯一真源，enum/名字表/扫描码表全部由它生成）：

| 文件 | 内容 |
|---|---|
| `src/keys.h` | `fr_key` 便携键 id、`FR_KEY_LIST`（id、规范名、BIOS set-1 make code、`0xE0` 标志）、查询 API |
| `src/keys.c` | 由 `FR_KEY_LIST` 生成的表 + `fr_key_name/scan/extended/by_name/from_scan`（**零 OS 依赖**） |
| `src/keys_win32.c` | `fr_key <-> VK` 桥（只有这个文件知道 `windows.h`；`main_win32.c`/`main_sokol.c` 照旧用 `MapVirtualKeyA`） |
| `src/keyscheck.c` | **对拍判据**（见下），`build.ps1 -Target keyscheck` |

**`keyscheck` 判据**（Windows 参考 = 现有入口层用的 `MapVirtualKeyA`）：

1. `FR_KEY_LIST` 行数 == enum 个数；
2. 每个键：`MapVirtualKeyA(fr_key_vk(key), VK_TO_VSC) == 表里的 scan`；
3. 名字双向可查（`fr_key_name` / `fr_key_by_name` 自洽）；
4. 反向 `from_vk`/`from_scan` 的 scan 必须一致，共享 make code 的（KP7/HOME、
   RETURN/KPENTER、LCTRL/RCTRL、SLASH/KPDIV、LALT/RALT）记为 **alias**，不算失败；
5. `ext` 标志 == 入口层 `is_extended_vk()` 的 `0xE0` 规则。

**结果**：`keyscheck: 102 keys pinned against MapVirtualKeyA, 5 allowed aliases, PASS`（exit 0）。
`--dump` 打全表（Linux 侧的参考）。

**测试当场抓到的两件事**（都记进 `PITFALLS.md` §8-64）：

- `fr_key_from_vk` **必须先查 VK 表再按字母范围判断**：`VK_F1=0x70='p'`、
  `VK_MULTIPLY=0x6A='j'`、`VK_NUMPAD7=0x67='g'`，否则功能键/小键盘被当成字母。
- `MapVirtualKeyA` 对 `VK_SNAPSHOT`/`VK_PAUSE` 没有可用 make code（实测 **0x54** 和 **0**），
  而 guest 根本收不到这两个键 ⇒ 表里**干脆不放**（`ext` 也因此不是"裸 8042 流"，
  而是"入口层实际发给 guest 的 `0xE0` 规则"：只有方向/Home/End/PgUp/PgDn/Ins/Del 置位）。

**为什么 `ext` 用入口层规则而不是裸 BIOS 规则**：`main_win32.c`/`main_sokol.c` 的
`is_extended_vk()` 只对那 10 个导航键放 `0xE0`，RCTRL/RALT/KPENTER/LWIN… 一律
`ToAscii` 的结果。Linux 层要和 Windows **产出同一个 `(scan, ascii)`**，所以表以入口层规则为准；
`keyscheck` 第 5 条把这个等式钉住。

## 46.4 本轮还没做（下一刀）

1. 把 `host.c`/`keylog.c`/`main_win32.c`/`main_sokol.c` 接到 `keys.[ch]` 上：
   `input_post_vk` → `input_post_key(fr_key)`，`vk_from_name` 退化成 `fr_key_by_name`；
   `main_sokol.c` 的 `sapp_to_vk` 换成 `SAPP_KEYCODE -> fr_key`，字符用 `SAPP_EVENTTYPE_CHAR`；
   回归判据：`regress.ps1` 8/8 + `--keylog`/`--keyplay` 同 tick 抓帧 **0 px**。
2. `platform.h` 第 3 切片 + `host.c` 过河（时间/线程/目录/文件属性/日志重定向）。
3. Linux `main_sokol.c` 去掉 Win32 分支（`#if defined(_WIN32)` 包住 HWND/winshot 部分），
   用 `Makefile.linux` 编 64 位宿主（此时还跑不了 guest，只能说"能编"）。
4. `-m32`：装 `gcc-multilib` + 32 位 X11/ALSA，跑真游戏；用 `faultprobe32` 复核
   §45.2 的故障表在"游戏真跑"现场仍成立。

## 46.5 第二切片：把便携键表接进宿主（已落）

**改动**（行为应逐位不变，判据见下）：

| 位置 | 改前 | 改后 |
|---|---|---|
| `host.h` | `input_post_vk(int vk)` | `input_post_key(fr_key)`（`host.h` 现在 include `keys.h`） |
| `host.c` | 自带 `vk_from_name()` 表 + autokey 发 VK | 删表，autokey 用 `fr_key_by_name()` + `input_post_key()` |
| `keylog.c` | `MapVirtualKeyA(scan,VSC_TO_VK)` → 自带 VK 名表；回放 `input_post_vk` | 记录走 `fr_key_from_scan(scan, ascii==0xE0)`；回放 `fr_key_by_name`/`input_post_key`；`keylog_note(scan, ascii)` |
| `main_win32.c` / `main_sokol.c` | `input_post_vk(vk)` | `input_post_key(fr_key)` → `fr_key_vk()` → 原 `MapVirtualKeyA`/`ToAscii` 路径（一字未动） |
| `keys_win32.c` | — | 追加 3 个 legacy alias（`VK_SHIFT/VK_CONTROL/VK_MENU`），使旧录制里的 `#16/#17/#18` 仍可回放 |
| `build.ps1` | — | `fd2host` 加 `keys.c` / `keys_win32.c` |

**为什么用 `ascii` 反推扩展键**：`keylog` 只存扫描码，而 KP7/HOME、KP8/UP 等共享 make code。
旧实现靠 `MapVirtualKeyA(scan,VSC_TO_VK)` 猜（`keyscheck --vsc` 实测它在导航簇里**偏向扩展解释**，
所以旧录制里箭头就是 `UP/DOWN/HOME`）；新实现直接用入口层放进 BDA 的那个 `0xE0` 标志，
更准且**跨平台**（Linux 没有 VSC_TO_VK）。

**判据（本轮实测）**：

| 判据 | 命令 | 结果 |
|---|---|---|
| 键表对拍 | `build\keyscheck.exe` | `102 keys … 5 allowed aliases … PASS`（退出码 0） |
| 两入口层都能编 | `aux_build.bat fd2host` / `fd2host -Render gdi` | 均 `-> build\fd2host.exe` |
| 回归 | `regress.ps1` | **8/8 PASS**，`FD2.TMP = 207360` |
| 录制↔回放 | 沙箱 `--autokey … --keylog=keys_ref.txt --shot-tick=600` → `--keyplay=keys_ref.txt` 同 tick | **0 / 64000 px**；录制名 `SPACE/RETURN/DOWN` 与改前格式一致 |
| 旧格式兼容 | `printf '3000:#16\n4000:SPACE\n' > legacy.txt` 后 `--keyplay` | `#16` 解析为 `LSHIFT`（不再是 `unknown key`），回放 2/2 |

**顺带踩坑**：`keys_win32.c` 的块注释里写了 `VK_L*/VK_R*`，`*/` 提前结束注释导致编译失败
（`PITFALLS` §8-65）。

## 46.6 第三切片：`platform.h` 第 3 切片 + `host.c`/`keylog.c` 过河（已落）

**目标**：把 `host.c`/`keylog.c` 里最后的 Win32 直连换成 platform.h 缝，让"宿主内核"两边同源。

**新增缝**（`platform.h` slice 3；Win32/POSIX 各一份实现）：

| 函数 | Win32 | POSIX |
|---|---|---|
| `plat_now_ms()` | `GetTickCount64` | `CLOCK_MONOTONIC` |
| `plat_thread_stk(fn,arg,stack)` | `CreateThread(NULL, stack, ...)` | `pthread_attr_setstacksize` |
| `plat_set_cwd` | `SetCurrentDirectoryA` | `chdir` |
| `plat_module_path` | `GetModuleFileNameA` | `readlink("/proc/self/exe")` |
| `plat_path_size` | `FindFirstFileA`（目录元数据，游戏还开着也能读） | `stat` |
| `plat_stricmp` / `plat_strdup` | `_stricmp` / `_strdup` | `strcasecmp` / `strdup` |
| `plat_stdio_pin` | `_dup2(_fileno(stdout),1/2)` | 空操作 |

**host.c 的改动**：删 `#include <windows.h>`/`<io.h>`；`DWORD/HANDLE/WIN32_FIND_DATAA`→
`uint64_t`/`plat_*`；`DWORD WINAPI xxx(LPVOID)` → `void xxx(void*)` + `plat_thread*`；
`__cdecl` → `FD2_CDECL` 宏（x86-64 无此关键字）；`MAX_PATH` → `PLAT_MAX_PATH`；
路径分隔符两种都认（`path_last_sep()`）；**BMP 头改为手写 54 字节小端**（不再依赖
`BITMAPFILEHEADER/BITMAPINFOHEADER`）；`GetModuleHandleA(NULL)` → `plat_image_base()`。

**keylog.c 的改动**：删 `windows.h`；`DWORD`→`uint64_t`；`CreateThread/WaitForSingleObject`
→ `plat_thread` + `volatile int g_playing`（线程结束时清零，`keylog_replaying()` 读它）；
`GetTickCount`→`plat_now_ms`；`GetModuleFileNameA`→`plat_module_path`。

**判据（本轮实测）**：

| 判据 | 结果 |
|---|---|
| Windows：GDI 与 sokol 两个入口层 | 均编译通过（0 error） |
| `regress.ps1` | **8/8 PASS**，`FD2.TMP = 207360` |
| 录制↔回放同 tick | **0 / 64000 px** |
| Linux：`make -f Makefile.linux` + `letest-linux` | 编译通过，`reference check OK, exact match`（三对象） |
| Linux：`doscheck-linux` / `platprobe-linux` | **49/49 PASS** / exit 0 |

**顺带修**：`render_sokol.c` 的 `sokol: backend=...` 日志少一个 `%d`（MSVC C4474），补上
`img=%d`。

**此时两边的分工**：`host.c`/`keylog.c`/`dos.c`/`le.c`/`keys.c` 已零 `windows.h`
（`keys_win32.c` 是唯一带 VK 的文件，只给 Windows 入口层用）；还剩 **入口层**
（`main_sokol.c` 去 Win32、`winshot.c`）、**AIL 栈**与 **`-m32`**。

## 46.7 第四切片：Linux 宿主（入口层 + 音频栈过河 + `fd2host-linux` 链接）（已落）

**入口层**（`src/main_sokol.c`）按平台分两段：

| | Windows（不变） | POSIX（新） |
|---|---|---|
| 键码 | `sapp_keycode → VK`（保留原表） | **`sapp_keycode → fr_key`**（`src/keys.h`，XKB 布局无关） |
| ascii | `ToAscii` | **`SAPP_EVENTTYPE_CHAR`**（sokol 的 X11 后端内部就是 `XLookupString`）；用 `host_key_set_last_ascii()` 回填刚写的 make code（仅在 guest 未取走时生效，见 `host.c`） |
| 注入 `--autokey/--keyplay` | `fr_key → VK → push_vk` | `fr_key → (scan, ascii=fr_key_ascii)` 直接 `host_key`（不需要窗口） |
| `--wshot` | `winshot_capture`（`PrintWindow`） | 打印"不支持，用 `--screenshot`"（后者与后端无关） |

**音频/AIL 栈过河**（`platform.h` slice 4）：

- 新增 `plat_mutex`（Win `CRITICAL_SECTION` / POSIX 递归 `pthread_mutex`）、
  `plat_atomic_read/write/inc`（`Interlocked*` / `__atomic_*`）、`plat_now_us`（QPC / `CLOCK_MONOTONIC`）。
- `audio_sokol.c`/`ail.c`/`synth.c`/`xmidi.c`/`dls.c`/`repl.c` 去掉 `windows.h`：
  `CRITICAL_SECTION→plat_mutex`、`Interlocked*→plat_atomic_*`、`GetTickCount/QPC→plat_now_ms/us`、
  `CreateThread/WaitForSingleObject→plat_thread + 完成标志`、`_snprintf→snprintf`、
  `__cdecl→PLAT_CDECL`、`_stricmp→plat_stricmp`；`xmidi.c` 的 winmm 后端（`midiOut*`）用
  `#if defined(_WIN32)` 包住，POSIX 下 `g_midi` 恒 NULL（默认后端本来就是内置合成器）。

**编译期发现的真问题**：`src/game/svc.c` 用 MSVC 关键字 `__cdecl` 且缺 `uintptr_t` 的来源
⇒ 在 Linux 编译不过。改成 `PLAT_CDECL` + `#include "../platform.h"`。
（含义见 §46.8：**转译模块本身也回调原机器码的 32 位地址**，所以整机必须 32 位。）

**构建**（`Makefile.linux`）：

```
make -f Makefile.linux build/fd2host-linux   # 64 位：全部 POSIX 宿主源码编译+链接通过（不能跑 guest）
make -f Makefile.linux host32                # 真能跑的宿主（-m32），需要 i386 工具链/库
```

**判据（本轮实测）**：

| 判据 | 结果 |
|---|---|
| Windows：`fd2host`（sokol）、`typecheck` | 0 error；`typecheck` **1616 例 0 失败** |
| Windows：`regress.ps1` | **8/8 PASS**，`FD2.TMP = 207360`；日志 `audio: device closed (331776 frames mixed)` |
| Linux：`src/game/*.c` 全部单独编译 | 无错误 |
| Linux：`build/fd2host-linux` | **链接通过**（`-lX11 -lXi -lXcursor -lGL -lasound -ldl -lm`），0 warning |
| Linux：`letest-linux` / `doscheck-linux` | 仍 exact match / **49/49** |

**唯一阻塞（需要人工/带 sudo）**：本机 WSL 是普通用户、`sudo` 要密码，装不了 i386 工具链 ⇒
`make host32` 暂时只能停在"缺 `bits/libc-header-start.h`"。装完即可跑真游戏（见 §46.8）：

```bash
sudo apt-get install gcc-multilib libc6-dev-i386 \
  libx11-dev:i386 libxi-dev:i386 libxcursor-dev:i386 \
  libgl1-mesa-dev:i386 libasound2-dev:i386
```

## 46.8 为什么"已经源码化了"仍然要 32 位（写下来免得再被误解）

`re/funcmap.csv` 共 **1359** 个函数，`src/repl.c` 只接入 **51 个（≈3.8%）**；**其余 ~96% 还是
`FD2.EXE` 的原始 32 位机器码**，由宿主进程直接执行。更关键的是：
**连这 51 个已转译的 C 函数也不独立** —— 它们会回调仍是机器码的兄弟函数
（`src/game/svc.c` 的 `ORIG_TICK/ORIG_INIT/ORIG_ADDR/ORIG_LOOP/ORIG_START` 就是 `(uintptr_t)0x4E310`…
这样的**固定 32 位地址**），而且游戏数据段也是按 32 位扁平地址写死的。
⇒ **整个宿主进程必须在 32 位模式下运行**（Windows WOW64 / Linux `-m32`），
`fd2host-linux` 那份 64 位产物只能证明"POSIX 侧源码全部编译/链接通过"，跑不了 guest。
**等 1359 个函数全部源码化、不再有人回调机器码，这个门槛才消失。**

## 46.9 下一步

Linux 侧已到"只差 32 位环境"：

1. **用户执行一次** `sudo apt-get install gcc-multilib libc6-dev-i386 libx11-dev:i386
   libxi-dev:i386 libxcursor-dev:i386 libgl1-mesa-dev:i386 libasound2-dev:i386`，
   然后 `make -f Makefile.linux host32` → `build/fd2host-linux32`，在 WSLg 的 XWayland 里
   跑真游戏，与 Windows 同 tick 抓帧对拍（`--screenshot` 与后端无关）。
2. 跑通后重跑 `faultprobe32`，复核 `rounds/15` §45.2 的故障表在**游戏真跑**的现场仍成立。
3. **工作重心转回源码化**（用户决定）：按 `docs/TRANSLATION.md` §5 继续把 1359 个函数里的机器码
   换成 C（当前 51）。Linux 那两刀不再往前推（Wayland 原生明确不做）。

## 46.10 Linux 32 位宿主真跑：与 Windows 同 tick **0 像素**差异（2026-10-07）

用户装好 i386 工具链后，`make -f Makefile.linux host32` 通过（修掉 `dos_fault_posix.c` 的
i386 `mcontext_t` 与 `plat_data_selector` 的 asm 约束，见 `PITFALLS` §8-66），
`build/fd2host-linux32` 在 **WSLg 的 XWayland** 上跑起了真游戏：

```
host: address space reserved
le: fixups applied=7948, out-of-page sources=11, ... bad records=0
dos: redirected 65 low-memory references
ail: patched 52 AIL entry points (FD2 layout) ...
audio: saudio_setup(22050 Hz) failed - no sound this run   (WSLg 无 ALSA 设备，宿主继续)
sokol: backend=GLCORE image=320x200 ...
host: entering game code at 0x3CCB4
dos: INT10 set video mode 0x13
host: watchdog fired after 8 s (12225 frames drawn) - 1526.6 fps
```

**判据（跨平台端到端）**：同一 `--gamedir=build/sandbox` + 标准 autokey + `--shot-tick=600`：

| 运行 | 结果 |
|---|---|
| Linux `fd2host-linux32`（GLCORE/XWayland） | `frame 53046 … guest tick 600` → `build/lx.bmp` |
| Windows `fd2host.exe`（sokol/D3D11） | `frame 5924 … guest tick 600` → `build/win.bmp` |
| `framediff.ps1 win.bmp lx.bmp` | **0 / 64000 px（0.0000%），max channel delta 0** |

⇒ 从 DOS/4GW 机器码到 Linux 屏幕这条链（LE 加载 → fixup → VEH/sigaction 故障分发 → INT 10h/21h/31h
→ 调色板 → GLCORE 呈现）在 Linux 上**与 Windows 逐像素一致**。`--screenshot` 与后端无关，
所以这就是可用的跨平台画面判据。

## 46.11 32 位只是"对照载具"：通往 64 位 / macOS 的纯 C 路线

用户明确了终局要求：**全部逆向成 C 后不要再用 32 位**（32 位会被淘汰，macOS 从 Catalina 起
已彻底不支持 32 位）。这与项目目标一致，路线如下（也写进 `docs/TRANSLATION.md` §6）：

| 阶段 | 内容 | 32 位？ |
|---|---|---|
| A（现在） | 32 位宿主 + 原机器码；转译出的 C 经 `repl.c` 接入，用**原机器码**做逐字节对拍 | **必须 32 位**（x86-64 长模式不能执行 32 位代码，C 还要回调机器码的固定地址） |
| B | 把 1359 个函数**全部**转译完（现 51），机器码不再被执行 | 仍需 32 位做对拍 |
| C | **去 guest 化**：把 C 里的绝对地址/`uintptr_t` guest 指针/`ORIG_*` 回调改成真正的结构体与指针；删掉 LE 加载器、DOS 服务层、AIL 替换层，改成一个普通引擎层 | 这时才**不需要** 32 位 |
| D | 64 位跨平台构建：Windows D3D11 / Linux GL / **macOS Metal（需补 MSL shader，或用 sokol-shdc）**；音频走 sokol_audio 的 WASAPI/ALSA/CoreAudio | 64 位原生产物 |

**过渡期怎么少欠账**：新转译的模块尽量"少硬编码 guest 绝对地址"——需要读的全局集中到
少数访问器（`tables.c` 已经这么做了），这样阶段 C 的改动量可控。
