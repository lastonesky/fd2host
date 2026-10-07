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
