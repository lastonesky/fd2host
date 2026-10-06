# 轮次明细：按键录制与回放（`--keylog` / `--keyplay`）
§42 用户操作可复现化：把"我这边点了什么、第几毫秒"变成我能直接跑的输入。
---

## 42. 第 42 轮：按键录制与回放（2026-10-06）

### 42.1 需求

用户玩的时候发现 BUG，但**我的复现不了他的操作**（我只能跑 `--autokey` 的固定时序）。
需要：**记录"进程启动后第几毫秒、按了哪个键"**，然后我能用这份记录重新跑一遍。

### 42.2 设计（三条约束决定了实现）

| 约束 | 结论 |
|---|---|
| 要两个渲染后端通吃、真实键盘与注入都要记 | 记在 **`host_key()`** —— 它是两条后端 × 真实/注入的**唯一漏斗**（`main_win32.c` 的 `WM_KEYDOWN`、`main_sokol.c` 的 key 事件、两边的 `input_post_vk` 全部汇到这里） |
| 时间要能和 `--shot-time`/`--exit-after` 对齐 | 用 **`GetTickCount() - g_start_tick`**，即**自进程启动的绝对毫秒**（正是用户要的"进程启动多久"） |
| 录完必须能原样重放 | 文本格式 `<ms>:<键名>`，键名用 `vk_from_name` 认的那套 ⇒ 自动往返；表里没有的键写 `#<十进制 VK>`，解析端也认 ⇒ **任何键都能回放** |

**与 `--autokey` 不通用**（这点必须写清楚）：`--autokey` 的延时是**相对上一步**的，
`--keyplay` 的时间是**绝对**的 ⇒ 录制文件要用 `--keyplay` 回放，不能塞进 `--autokey`。

### 42.3 架构：功能不进 `host.c`（用户当场指出）

录制/回放是**诊断功能，不是宿主逻辑**，全部放进 `src/keylog.c` + `src/keylog.h`，
`host.c` 只留 5 个调用点：

```c
keylog_init(g_keylog_path, g_keyplay, g_start_tick);   /* host_init  : 开文件+读表 */
keylog_note(scan);                                     /* host_key   : 每个 make 码 */
if (keylog_start()) { … } else if (g_autokey …)         /* host_start : 起回放线程   */
if (g_autokey_done && !keylog_replaying() && exit_file_ok())  /* 退出触发要等回放 */
keylog_finish();                                       /* 两条退出路径各调一次      */
```

回放线程需要 `input_post_vk()`（入口层 API，`host.h` 里），所以 `keylog.c` 只 `#include "host.h"`
—— 依赖方向干净：**host.c 依赖 keylog，keylog 不依赖 host.c 的任何 static**。

两个退出路径都要调 `keylog_finish()`：watchdog 走 `ExitProcess`、根本不跑 `host_shutdown`
（§8-56 的教训，这次直接按规则挂两处）。

### 42.4 判据

| 项 | 结果 |
|---|---|
| **录制**（给定已知时序） | `--autokey=5000:SPACE;2500:RETURN;…` → 录到 `5125:SPACE / 7641:RETURN / 10157:RETURN / 12657:DOWN+RETURN`（绝对时间 = 累积延时 + 线程启动抖动 ✓；`DOWN,RETURN` 同毫秒 = 同一行两条 ✓） |
| **回放往返** | 用录下来的文件 `--keyplay=…` 再跑一次，同 tick 抓帧 vs 录制那一次：**0 / 64000 px** |
| 崩溃也留证据 | 逐键 `fflush` 写文件；日志里每键一行 `host: key @ms VK`，收尾再打一条可直接回放的 `host: key schedule (…): 5141:SPACE;…` |
| 回归 | `regress.ps1` **ALL PASS（8/8）**、`FD2.TMP = 207360`、`ail: play 16` |

### 42.5 顺带查清的一件事：启动瞬间会"吃"到别处的键

第一次录制里出现了**我没注入的** `188:B`、`250:A`。查证：

- 时刻正好是窗口出现（`main_win32.c:172 ShowWindow(SW_SHOW)` / sokol 同样）；
- 机制：**新窗口一显示就抢前台焦点**，那一刻你在别的窗口敲的键落进了游戏窗口；
- 这正是既有选项 `--no-user-input`（`ENVIRONMENT.md`）针对的场景，护栏已经在；
- 第二次录制（没人在打字）就只有注入的 5 个键 ✓ 佐证。

⇒ 已记 `PITFALLS` §8-57：**录手动会话时，窗口出现前后别碰键盘**，或先确认焦点；
日志里这几个早期键一眼可见（`host: key @1xx ms …`），删掉即可。

### 42.6 用法（给以后的 agent/用户）

```powershell
# 录：我玩，出问题后把这份文件给我
Start-Process E:\FD2\port\build\fd2host.exe -ArgumentList `
  '--keylog=E:\FD2\port\build\keys.txt' -WorkingDirectory 'E:\FD2'

# 回放：用同一条命令复现你的操作（不加 --autokey）
Start-Process E:\FD2\port\build\fd2host.exe -ArgumentList `
  '--keyplay=E:\FD2\port\build\keys.txt','--screenshot=E:\FD2\port\build\repro.bmp',`
  '--shot-tick=600','--exit-when-file=E:\FD2\port\build\repro.bmp:256054' `
  -WorkingDirectory 'E:\FD2'
```

不给 `--keylog` 也照样记：每键一行进 `host.log`，收尾有完整 `key schedule` 行。

### 42.7 下轮入口

`PROGRESS.md` 下一步：第 5 条（稳定性长跑）、第 6 条（存档路径，只在沙箱）、第 7 条（跨平台）。
