# FDPS（炎龙外传）冻结存档
**已冻结（2026-10-05 用户决定不再继续支持）**。这里保留已达成的成果与剩余卡点，
只作参考不再投入；宿主的通用能力（`--exe`、FDPS AIL 表、定时器线程、INT9 注入）仍在代码里。
对应旧 `PROGRESS.md` §14–§18。
---

## 14. 第 14 轮：平台通用化（`--exe` 可跑任意 LE 游戏）+ 炎龙外传 FDPS 首跑（2026-10-05）

**目标**：把宿主从“FD2 专用”改成“通用 DOS/4GW(LE) 宿主”，验收 = 能加载并运行同系列的
《炎龙骑士团外传》`E:\Games\FDCollection\Game\FDPS\FDPS.EXE`（用 `--exe` / `--gamedir` 指定）。

### 14.1 先做静态体检（不运行，零风险）

```powershell
python E:\FD2\port\re\preflight.py "E:\Games\FDCollection\Game\FDPS\FDPS.EXE"
```
输出（摘要）：
```
LE header @0x2A50  77 pages  3 objects  entry=0x43008
obj0 0x10000 (0x479D0)   obj1 0x60000 (0xC3C0)   obj2 0x70000 (0x54)
CONFLICT obj2 overlaps low-memory mirror (BDA/PSP/IVT)
AIL: FOUND AIL_startup, AIL_shutdown, AIL_install_DIG_INI, AIL_set_preference
extender: RATIONAL DOS/4G, WATCOM
```
⇒ 开跑前就已知两个必修点：**低内存镜像冲突**、**AIL 52 个地址是 FD2 专属（不能乱打补丁）**。

### 14.2 十个卡点与修复（每条都有日志判据，细节见 §8-34..40）

| # | 卡点 | 日志判据（修复前） | 修复 |
|---|---|---|---|
| 1 | AIL 52 个入口地址是 FD2 的 | 静态：FDPS 含 AIL trace 串 → 会被写进错误地址 | `--ail=auto\|fd2\|none`，auto 只对 `FD2.EXE` 打补丁 |
| 2 | 低内存镜像写死 `0x70000`（FDPS obj2 正好在那里） | `dos: cannot map low-memory window` | `dos_choose_lowmem()` 按对象表挪镜像 |
| 3 | `VirtualAlloc` 64 KiB 粒度 | ERROR **487** | 镜像 64 KiB 对齐 + 实模式池让位（§8-34） |
| 4 | 只预留 `0x10000..0x6FFFF` | CRT 抢走 `0x80000`/VGA → 镜像失败 → `files_init()` 没跑 + `host_frame` 读 `0xA1000` 崩 | 改为**一次预留 `0x10000..0x100000`**；因低内存偶发被加载器占用，改成**逐 64 KiB 块预留** + **按区域逐段 commit**（`le_commit_range()`，§8-41） |
| 5 | GUI 进程无 fd1，游戏 printf 全丢 | `dos: write h=1 want=39 n=0`（错误 6） | `_dup2(_fileno(stdout),1)` + `_get_osfhandle()`（§8-35） |
| 6 | 映像数据起点算法（末对象 BSS 尾） | 执行错位指令流：`mov es,[ebx]` `EBX=0`、`int` 服务数 = 0 | 用 LE 头 `+0x2C`（§8-36）；IDA 入口字节对拍定起点 |
| 7 | fixup 类型 `0x02` 未识别 | `bad records=1, leftover=1` → 该页丢 987 字节（140 条） | 实现 `0x02`（5 字节、无目标偏移）（§8-37） |
| 8 | `INT 21h AH=43h` 未实现 | `UNHANDLED INT21 AH=43` → `access("DISK.NO")` 失败 → `exit(1)` | 实现 get/set attributes（§8-40） |
| 9 | `INT 2Fh AX=1500h` 未实现 | `Fatal error: CDROM is not install!!!` → `exit(1)` | 回 `AL=FFh, BX=0x0210`（§8-39） |
| 10 | `0x3DA` 恒返回 `0x09` | 画面停第一帧 + **端口操作 2700 万次**（死循环读状态口） | 状态位每次读翻转 `0x09↔0x00`（§8-38） |

> 新增诊断：`AH=3F/40` 对 **≤512 字节**的小传输打日志（前 40 条，含内容）——
> “游戏到底读到/写出什么”第一次变得可见；`INT10` 非 `0x13` 模式会告警（宿主仍按 320×200 显示）。

### 14.3 FDPS 现状（实测）

```
加载   ✓  LE 77 页解析；fixup 6831 / bad=0 / leftover=0（补上 0x02 后恢复 140 条）
       ✓  低内存镜像自动 0x70000 → 0x80000（seg 0x8000），20 处低内存引用重定向
       ✓  `int` 服务数 0 → 启动后正常计数
启动链 ✓  AH=30 版本探测 → AH=4A 调内存 → 低内存模拟读 PSP:0x2C/0x80 → MSCDEX 检测
       ✓  DIG.INI 打开、SB16.DIG 被 16 位驱动拦截、MISC.VFS 读取
渲染   ✓  `INT10 set video mode 0x13`、调色板写入（标准 EGA 16 色，DAC 日志可见）
       ✓  标题帧 blit 到 0xA0000（帧非黑、18 种颜色），连续 45–120 s 无崩溃
卡点   ✗  `AIL_register_timer`（`sub_3DF06`，靠它自己的 trace 串定位）注册成功但**回调永不触发**
         → 动画时钟 `dword_69D64`（156 处引用）不走 → 标题循环停在第一帧
```

### 14.4 FD2 未被破坏

`regress.ps1` **8/8 PASS**（45 s 与 75 s 各一次；中间一次 45 s 失败是**机器负载的时序抖动**
（Defender/Code/IDA 同时占用），默认已从 45 s 调到 **60 s**）。日志证明 FD2 **不调用** `AH=43`、
**不调用** `INT 2F/1500`，因此新增逻辑对它零影响；`0x3DA` 翻转只会让等待循环更快结束。

### 14.5 下一轮入口：FDPS 的 AIL 定时器（进而是它的音效/音乐）

> **加载方式已实测（2026-10-05）**：不需要手动在 IDA GUI 里加载 FDPS.EXE。ida MCP 的
> `open_database("E:\\Games\\FDCollection\\Game\\FDPS\\FDPS.EXE")` 直接以 idalib 无头跑完自动分析
> （1353 函数 / 437 串 / 入口 `start`=0x43008 = `preflight.py` 的 `entry=0x43008`），随后
> `save_database()` 落盘 `FDPS.EXE.i64`；**下一轮直接开 .i64，秒级跳过重新分析**。
> 多库切换：`execute_python(instance_id=...)` 或先 `list_databases()`；当前默认 target 会被新打开的库顶掉。
> AIL trace 串已在库里，首条证据已到手：
> `AIL_register_timer(0x%X)\n`@`0x62508` ← xref `0x3df51`（即 §14.3 的 `sub_3DF06`）；
> `AIL_set_timer_period(%u,%u)\n`@`0x6253d` ← `0x3e133`；`AIL_start_timer(%u)\n`@`0x625b1` ← `0x3e36d`。
> ⇒ **定时器族入口集中在 `0x3DF00..0x3E400` 一带**，第 1 步的“反查入口表”可直接从这里续做。

1. 用 ida MCP 按 **AIL trace 串**（`"AIL_register_timer(0x%X)\n"` 等）反查 FDPS 的 AIL 入口地址表 ——
   与 FD2 当年建 52 条表的手法相同（`re/RE_MAP.md` §3、docs/ENVIRONMENT.md §10）。
2. 在 `ail.c` 实现定时器族：`AIL_register_timer`（存回调）、`AIL_set_timer_period/frequency`、
   `AIL_start/stop_timer`、`AIL_release_timer_handle`；宿主起一个高精度线程按周期**直接调用 guest 回调**
   （回调是普通近函数 `sub_30520: inc dword_69D64; call rand; ret`，在宿主线程执行即可，
   不碰游戏线程的寄存器；只需注意 `rand()` 的共享状态并发）。
3. 同一张表顺带把 FDPS 的**数字音效/音乐**接上（`*.DIG/*.MDI` 已在磁盘上，当前被当 FD2 处理拦截）。
4. 工具与存档：`re/preflight.py`（静态体检）、`re/fixup_scan.py`（fixup 语法核对）、
   `re/fdps_*.c`（本次反编译存档：`main`、`sub_2A280` 标题循环、`sub_3C3A6` CD 检测、`sub_3DF06` 等）。

> **第 15 轮已完成上面第 1、2 步（入口表 + 定时器族），见 §15**；第 3 步的样本侧 4 个入口也已接上，
> 剩下的音效实测被 `AH=4B` 挡在标题 → spawn 循环里（§15.6）。

---

## 15. 第 15 轮：FDPS 的 AIL 入口表 + 定时器族（25 Hz 动画时钟打通）（2026-10-05）

**目标**（= §14.5 的第 1、2 步）：给 FDPS 建它自己的 AIL 入口表，在 `ail.c` 实现定时器族，
让宿主线程按周期**直接调用 guest 回调**，动画时钟 `dword_69D64` 走起来、标题不再卡第一帧。

### 15.1 开工前的前提：IDA 库已就位，不用手动加载

见 §14.5 的实测注记：`open_database(FDPS.EXE)` → idalib 无头自动分析 → `save_database()`
落盘 `E:\Games\FDCollection\Game\FDPS\FDPS.EXE.i64`（1353 函数 / 437 串 / 入口 `0x43008`）。
本轮所有反查都在这个库上跑，**没有手动在 IDA GUI 里加载过任何东西**。

### 15.2 入口表怎么建的（判据可复现）

| 步骤 | 结果 | 产物 |
|---|---|---|
| 扫 `AIL_xxx(` trace 串 → 串的代码 xref → 所在函数 | 107 条串 → **90 个不同函数** | `re/fdps_ail_trace.csv` |
| 全量扫 `call`/`jmp`，目标落在 AIL 公共区 `0x3D488..0x41FFE` | **52 个 call 目标** = 47 个公共入口 + 5 个内部工具（`0x3D7B4`/`0x3D7B9` 加解锁、`0x3DA20`/`0x3DA25`/`0x3DBA8`） | `re/fdps_ail_patchset.csv` |
| 只看"游戏侧"（调用者 < `0x3D488`） | **18 个入口**是游戏真正调的 | `re/fdps_ail_gamecalls.csv` |
| 补丁地址两两间距检查 | 90 个地址 **0 处 <5 字节**（5 字节 `jmp` 不会互踩） | `re/fdps_ail_table.inc` |

- 5 个内部工具**不打补丁**（它们只被 AIL 自己的包装函数调用，包装已被打桩）；其余 90 个全打。
- 18 个游戏入口接真实现，其余 72 个接 `host_AIL_unused`（返回 0），保证**没有任何原版 AIL 代码可执行**。
- ⚠️ trace 串反查出的地址**不一定是入口**：IDA 把 `AIL_start_all_timers`/`AIL_resume_sequence` 等
  **别名**并进了前一个函数体（§8-42）。真入口一律以 `call` 目标为准，别名只用来起名字。

游戏侧那 18 个（宿主必须真实现）：
`startup / shutdown / install_DIG_INI / install_MDI_INI / allocate_sample_handle /
allocate_sequence_handle / init_sample / set_sample_address / set_sample_type /
start_sample / stop_sample / set_sample_playback_rate / set_sample_volume /
set_sample_loop_count / sample_status / register_timer / set_timer_frequency / start_timer`。

### 15.3 定时器语义（从 FDPS 自己的 AIL 反编译得到，不是猜的）

存档：`re/fdps_ail_AIL_*.c`（公共包装）、`re/fdps_core_*.c`（核心）、`re/fdps_timer_core_*.c`（ISR/编程）。

- **15 个槽，句柄 = 表内字节偏移**（`0,4,8,…,56`），`-1` = 无效；`AIL_register_timer` 满了回 `-1`
  （游戏 `sub_30540` 判 `== -1` 打 `"Timer fail !!!"`）。核心表：`used[15]`、`cb[15]`、
  `period[15]`、`counter[15]`、`pending[15]`、`user[15]`。
- 状态机：`0` 空闲 / `1` 已分配 / `2` 运行中（`start` 1→2、`stop` 2→1、`release` →0）。
- `AIL_set_timer_frequency(hz)` 就是 `set_period(1000000 / hz)`（原码 `0xF4240 / a2`）。
- 原版 ISR：`counter += 基准周期(所有活动定时器的最小 period)`，`>= period` 就 `pending++`，
  然后 `while (pending) { --pending; cb(user); }` ⇒ **停机后会追帧而不是丢帧**。
  宿主照抄这条语义，只把单次追帧上限设为 `AIL_TIMER_MAXPEND = 8`，避免长卡顿把游戏时钟一次推飞。
- **游戏侧只用 3 个**：`register_timer(sub_30520)` → `set_timer_frequency(h, 0x19)` → `start_timer(h)`，
  即**动画时钟 = 25 Hz**；回调 `sub_30520 = inc dword_69D64; call rand; ret`（`retn` = cdecl，
  多余的 user 参数无害）。

### 15.4 宿主实现（`src/ail.c`、`src/host.c`）

- **定时器子系统**：`CRITICAL_SECTION` + 一条 **1 ms tick 的宿主线程**（`QueryPerformanceCounter`
  计时，按每个定时器的 `period_us` 累加）；回调**在锁外**调用（guest 回调可能反手调 AIL）。
  `register_timer` 时惰性启动线程，`AIL_shutdown` 时停线程并清表。
- **样本侧补 4 个 FD2 没用到的入口**：`set_sample_type`（0/1/2/3 → 声道+位深，决定 waveOut 格式）、
  `set_sample_playback_rate`、`set_sample_volume`（先记录、全音量播，值打日志待定标）、
  `sample_status`（**回 4 = 空闲**，见 §8-44）。样本格式从"全局 `--ail-rate`"改成
  **每样本字段**（默认值仍取全局 ⇒ FD2 行为不变）。
- **句柄池 4 → 8**、`shutdown` 释放句柄（§8-43）。
- **按 exe 名选表**（`host.c`）：`FD2.EXE` → 52 条、`FDPS.EXE` → 90 条、其它 → 跳过并打印提示；
  `--ail=fd2` 强制 FD2 表、`--ail=none` 全关。

### 15.5 实测判据（`build/host.log`）

```
ail: patched 90 AIL entry points (FDPS layout) to host implementations (11025 Hz, 8-bit, 1 ch)
ail: register_timer(cb=00030520) -> handle 0        ← 没有 "Timer fail !!!"
ail: set_timer_frequency(h=0, 25 Hz -> period 40000 us)
ail: start_timer(h=0)
ail: timer fire #100 at +4000 ms                     ← 精确 25 Hz
ail: timer fire #1000 at +32625 ms                   ← 第二个音频会话，100 次 = 4000 ms 恒定
ail: shutdown (timer callbacks fired 185)            ← 第一个会话 ~7.4 s
```

- 45 s 内游戏**不再卡在第一帧**：读 `FDE.SAV`、二次写调色板、fade，随后走标题 → spawn 流程。
- `--screenshot --shot-frame=700` → `build/fdps_f700.bmp`：**82 种颜色**、非黑非单色（第 14 轮是 18 色首帧）。
- **FD2 未被破坏**：`regress.ps1` **8/8 PASS**，`FD2.TMP = 207360` 字节 = 原件同尺寸。

### 15.6 下一关口：`INT 21h AH=4B`（EXEC）

日志（本轮新增的诊断，会打印被 exec 的路径）：

```
dos: UNHANDLED INT21 AH=4B exec .\fd.exe (al=00 bx=61528)
```

反编译（`re/fdps_30CB0_spawn.c`，调用者 `0x1C233`/`0x2A533`）：`sub_30CB0` =

1. `AIL_shutdown` → 排空按键 → `sub_3C217()` → 调色板淡出；
2. `sprintf(v7, "%sFD.EXE", &unk_643E8)`、`v8/v9 = "%s<SVID/SAUD>"`（前缀是游戏目录字符串）；
3. **`spawnlp(0 /*P_WAIT*/, v7, v7, v8, v9, 0)`** —— 真正的游戏是同目录的 **`FD.EXE`**（125 KB），
   带 2 个命令行参数；CRT 走 Watcom `__dospawn` → `mov ah,4Bh`（`0x55FDB`）；
4. 子进程返回后：清屏 64000 字节 → 淡入 → `sub_30270(25)` **重新初始化音频**（这就是日志里第二次
   `ail: startup` 的来源）。

⇒ FDPS.EXE 自己只是**引子**：标题后 spawn 真游戏，等它退出再回来重开标题。
**AH=4B 不实现 = 标题 ↔ spawn 死循环**（未实现时 `set_cf(1)` 失败返回，游戏直接落到第 4 步）。

实现方向（下一轮）：
1. `dos.c` 实现 `AH=4B`：读 `DS:DX` 路径 + `ES:BX` 参数块里的命令行尾巴，
   **再拉起一个 `fd2host.exe --exe <path> --gamedir <cwd> --cmdtail=<尾巴>`** 并 `WaitForSingleObject`
   （`AL=0` 等待、`AL=1/3` 不等待），返回 `AL = 子进程退出码`、`CF=0`。
2. 宿主新增 `--cmdtail=`（或复用现成参数）把尾巴写进子进程的 **PSP:0x80**，`FD.EXE` 才拿得到
   SVID/SAUD 参数；父进程退出前要处理好 stdout/host.log —— 子进程会**覆盖**同一个 `host.log`
   （`freopen(...,"w")`），需要改成追加或按 PID 分文件，否则父进程日志被冲掉。
3. 现成的 AH=4B 诊断日志已经会打印路径与 `al/bx`，够定位参数块布局（下一步先反编译 `__dospawn`）。

---

## 16. 第 16 轮：`INT 21h AH=4B`（EXEC）打通 —— FDPS 真的把 FD.EXE 拉起来了（2026-10-05）

**目标**（= §15.6）：让 FDPS 标题流程的 `spawnlp(0, ".\FD.EXE", …)` 真的开得出进程，
否则 FDPS.EXE 永远在"标题 → spawn 失败 → 重新 init 音频"里打转，游戏本体 `FD.EXE` 一步都走不了。

### 16.1 约定与实现

| 项 | 做法 |
|---|---|
| 子进程是谁 | **再拉一个 `fd2host.exe`**：`--exe=<path> --gamedir=<父的 cwd> --log=<host.<pid>.log> --cmdtail=<尾巴> [--exit-after=<剩余秒>]`。不可能同进程加载第二个 LE（obj0 `0x10000` 已被父进程占用），而"父等子"正是 DOS EXEC 的语义 |
| 参数块布局 | `__dospawn`（`re/fdps_dospawn.c`）写的是 **offset:selector 成对**（本进程选择子基址为 0 ⇒ offset 即线性地址）：`+0 = env`、`+6 = 命令尾巴`；尾巴是 **`[len][chars][0x0D]`**（§8-45，按 C 串读会拖出 125 字节栈垃圾） |
| 等待语义 | `AL=0`(P_WAIT) → `WaitForSingleObject` + `GetExitCodeProcess`；`AL=1/3` 不等待 |
| `AH=4D` | 返回子进程退出码（`__dospawn` 在 exec 后紧接着调它取返回值） |
| 日志 | 新增 `--log=<path>`，AH=4B 给子进程发 `host.<pid>.log`（否则 `freopen("w")` 会清空父日志，§8-47）；父进程 watchdog 退出前先 `dos_terminate_child()` |
| PSP:0x80 | 新增 `--cmdtail=` → `dos_set_cmdtail()` 写 `[len][chars][0x0D]`；FD.EXE 靠它拿 `.\FD1.Vid` / `.\FD1.Aud` |
| 低内存串指令 | 新增 `emulate_lowmem_string()`：`A4..AF`（movs/stos/lods/cmps/scas，含 `rep/repe/repne`）整条在宿主侧跑完再跳过指令（§8-46） |
| 子进程限时 | `host_exit_after_remaining()` 把父进程 `--exit-after` 的**剩余秒数**传下去，避免父进程被看门狗杀掉留下孤儿 |

### 16.2 实测判据

父进程 `build/host.log`：

```
dos: INT 21h AH=4B exec al=0 '.\fd.exe' tail='.\FD1.Vid .\FD1.Aud'
dos:   child: "E:\FD2\port\build\fd2host.exe" --exe="...\fd.exe" --gamedir="E:\Games\FDCollection\Game\FDPS"
                 --log="...\host.31020.log" --cmdtail=".\FD1.Vid .\FD1.Aud" --exit-after=37
dos:   child exited with 0
```

子进程 `build/host.31020.log`：

```
dos: PSP:0x80 command tail (19 bytes) = '.\FD1.Vid .\FD1.Aud'
dos: lowmem string rep AE, 18 left (si=70000 di=82) at 0x1244C   ← REPE SCASB 跳过尾巴前导空格
dos: lowmem string rep A4, 0 left (si=94 di=32C63) at 0x12460    ← movsb 拷出 argv
...
ail: 'fd.exe' has no AIL table - skipping the hard-coded patches
dos: INT10 set video mode 0x13
dos: open '.\FD1.Aud' -> 00000338 (0)
dos: INT 21h AH=4Ch terminate, code=8
```

- 尾巴是 19 字节、内容逐字节正确；两条串指令模拟都是宿主侧一次跑完的。
- **FD2 未被破坏**：`regress.ps1` **8/8 PASS**（`FD2.TMP = 207360` = 原件同尺寸）。
- 实验环境：`build/fdps_sbx/`（游戏目录副本，**没动 `E:\Games` 下的原件**）。

### 16.3 当前卡点与下一步

1. **`FD1.Aud` / `FD1.Vid` 这两个文件整个 FDCollection 都不存在**：FD.EXE 读不到 → `exit(8)`；
   沙箱里给**空文件**照样 `exit(8)` ⇒ 文件内容有格式要求。反查起点：
   FDPS.EXE 的格式串 `'%s\%s.Vid'` / `'%s\%s.Aud'`（`0x61EE0` / `0x61EEC`），
   名字来自 `sprintf(v10, "FD%d", v27 + 1)`（`re/fdps_30CB0_spawn.c`）⇒ `FD1` / `FD2`。
   要么逆向 `FD.EXE` 看它怎么解析，要么找到生成它们的工具（同目录有 `SETSOUND.EXE`）。
2. **FD.EXE 还没有自己的 AIL 表**：子日志 `ail: 'fd.exe' has no AIL table` ⇒ 它跑的是原版 Miles AIL，
   FDPS 已经修好的"定时器不走 → 动画卡住"在子进程里会重现。下一步用 §15.2 同一套手法
   （trace 串 + 全量 `call` 扫描）给 FD.EXE 建表；先 `python re/preflight.py <FD.EXE>` 体检。
3. **`0x10000` 偶发被抢**（§8-48）：一次子进程启动失败（487，`type=MAPPED region=0x3000`），
   重跑即好；失败路径现在会用 `K32GetMappedFileNameA` 打出**映射的是哪个文件**，复现即可定位。

---

## 17. 下一轮入口调研（只读，未改代码）：FD.EXE 是过场播放器 / FDPS 自己挂 INT 9（2026-10-05）

第 16 轮跑通 exec 之后做了两件事：把子进程为什么 `exit(8)`、父进程为什么"画面定住且按键无效"
查到底。**两条根因都已定位，代码未动**（下面每条都有判据）。

### 17.1 FD.EXE = 过场动画播放器，缺的是 `.Vid`/`.Aud` 两个数据文件

IDA 库 `E:\Games\FDCollection\Game\FDPS\FD.EXE.i64`（`open_database` 自动分析，618 函数，
入口 `0x12280`，`preflight.py`：无预留冲突、`fixup bad=0 leftover=0`）。
`main`（存档 `re/fdexe_main.c`）：

```c
v4 = argv[1];                       /* ".\FD1.Vid" */
f  = fopen(argv[2], "rb");          /* ".\FD1.Aud"  */
if (!f) return 8;                   /* ← 文件不存在 */
len = filelength(...); if (len == 0) return 8;   /* ← 空文件，同样 8 */
buf = malloc(len); fread(buf, len);
obj = new(0x15D); sub_10420(obj, v4);            /* 解析 .Vid → 349 字节对象 */
sub_104D0(obj);                                  /* 初始化 */
sub_10B80(buf, 1, -1, -1);                       /* 播放 .Aud 音轨 */
loop: memcpy(0xA0000, obj+337, 64000); sub_10770(obj); ...   /* 逐帧 blit */
```

- 判据：子日志 `dos: open '.\FD1.Aud' -> FFFFFFFF (2)` → `AH=4Ch terminate, code=8`；
  沙箱里放**空文件** → `open -> 338 (0)` 之后**照样 `code=8`**（走了 `len==0` 分支）。
- `FD1.Vid`/`FD1.Aud`（以及 `FD2.*`）**整个 `E:\Games\FDCollection` 都不存在**，
  名字由父进程拼出：`'%s\%s.Vid'`/`'%s\%s.Aud'`（`0x61EE0`/`0x61EEC`）+ `sprintf(v10,"FD%d",v27+1)`。
- **结论：这份拷贝缺过场数据文件**，与宿主无关；缺了只是 intro 被跳过，
  父进程 `sub_30CB0` 返回后会继续 `byte_60008=1; sub_30960(1)` 进标题菜单（日志里第二次 `ail: startup` 即此）。

### 17.2 父进程标题菜单不响应键盘：**游戏自己挂了 INT 9，而宿主从不投递硬件中断**

画面证据（`--screenshot` 直方图对比）：

| 帧 | 颜色数 | 说明 |
|---|---|---|
| 150（≈5 s，spawn 前） | 23 | logo 阶段 |
| 400（≈12.5 s）/ 600（≈19 s）/ 750（含 3 次 autokey 之后） | 82，**三张逐像素直方图完全相同** | spawn 后进入标题菜单，之后画面与按键都不再变 |

机制（IDA 存档 `re/fdps_int9_56560.c`、`re/fdps_keyq_565A7.c`）：

```asm
sub_56560:  mov ax,3509h; int 21h          ; 取旧的 INT 9 向量 → 存 dword_70002/word_70000
           push cs; pop ds                  ; DS = CS（平坦，基址 0）
           mov edx, offset sub_565A7        ; ← ISR 本体
           mov ax,2509h; int 21h            ; 挂到 INT 9
sub_565A7: sti; in(0x60) → sc; in(0x61)/out(0x61) 应答
           if (sc < 0x80 && sc != last) byte_7000F[tail++] = sc   ; 10 项环形队列
           out(0x20,0x20); iret
sub_5652E: 出队（空队列返回 -1）            ; 菜单循环用它取键
```

- **ISR 没有任何 `call` 引用**（只被当向量装），所以静态扫描找不到调用者；
- 队列的唯一写入点就是这个 ISR ⇒ 宿主不投递 INT 9 → `sub_5652E()` 恒 `-1` → 菜单永远等不到键；
- 宿主侧 `dispatch_swint` 对 `0x08/0x09/0x1A` 是**直接忽略**（`case 0x09: g_calls[vec]++; break;`）；
- 实测：`--autokey` 打进去的 `RETURN/DOWN` 在日志里有 `host: autokey vk=0D (scan 1C)`，画面零变化。

### 17.3 下一轮的做法（顺序）

1. **投递 INT 9**：
   - `int21 AH=25 AL=09` 时记下 `g_guest_int9 = EDX`（**用完整 32 位 EDX**：`mov edx,imm32` 后
     DS=CS 基址 0，线性地址就是 EDX；别按 DX 截成 16 位）；
   - `host_key(scan)` → 置"待读扫描码"（让 `in 0x60` 返回它）→ 在宿主线程上**按中断帧调用 guest ISR**：
     栈上依次放 `EFLAGS、CS、返回地址` 再 `call ISR`，ISR 结尾的 `iret` 就正好弹回我们的返回地址
     （`iret` 在 ring3 是特权指令，VEH 里要补"弹 EIP/CS/EFLAGS 跳回"的模拟；`sti/cli/in/out` 已有模拟）；
   - 队列写完后菜单自然能读到键（`sub_5652E` 是游戏自己的代码，宿主不用碰它）。
2. **过场数据**：确认这批文件是不是要从 CD/别的拷贝补齐；没有就保持"跳过 intro"（现状已验证可用）。
3. **FD.EXE 的 AIL 表**：只有真要跑过场时才需要（§15.2 同一套手法）。

---

## 18. 第 18 轮：INT 9 投递打通（顺带揪出 fixup 写宽度 bug）（2026-10-05）

**目标**（= §17.3 第 1 步）：让 FDPS 标题菜单吃到按键。**结果：从标题菜单按 START NEW GAME
直接进了游戏场景**（`--screenshot` 前后两张图，见 §18.4），过程中挖出一个**从第 14 轮就在的加载器 bug**。

### 18.1 三条设计结论（每条都是被崩溃逼出来的）

| # | 错误做法 | 现象 | 正确做法 |
|---|---|---|---|
| 1 | 在**宿主线程**上 `pushfd/push cs/call ISR`，让它的 `iret` 弹回来 | `pop ds` 处 **#GP**（`read from address 0xFFFFFFFF`）。单步显示 `sti`~`in 61h` 之间栈凭空少了 **28 字节**——异常帧在“从不跑 guest 代码的线程”上没被回收 | **在跑 guest 代码的线程上注入真正的中断帧**：VEH 里 `Esp-=12` 写 `[EIP][CS][EFLAGS]`、`EIP=handler`，handler 自己的 `iret` 弹回被打断的指令 |
| 2 | 每次异常都检查“有没有排队的键”并注入 | 按键排队时 handler 还没跑完就再注入 → **return address 落在 handler 内部**，handler 从头重启、帧层层叠加 | `inject_int9()` 先判 `EIP ∈ [handler, handler+0x100)` → **在 handler 里绝不再注入**（相当于 PIC 等 IF 再置位） |
| 3 | BIOS 环形队列和游戏自己的队列**都写** | 游戏菜单一次按键走两遍（`cx=4141` 之类的脏值 + 跳飞） | 真机上游戏替换 INT9 后**不会链回 BIOS** ⇒ `dos_deliver_key()` 返回 1 就**不再写 0x41E**（`host_key` 直接 return） |

补充：注入必须落在**执行 guest 代码的线程**上（`dispatch_swint` 第一次跑到时记下 `g_guest_tid`），
否则又回到第 1 条；键在该线程的下一次异常（`int 21h` / 端口读都是异常）被投递，FDPS 每帧读
`0x3DA` ⇒ 延迟 <1 帧。

### 18.2 真正的根因：`type 0x02` fixup 写了 4 字节，源操作数只有 2 字节

第 14 轮为 FDPS 那条唯一的 `type 0x02` 记录加的处理是"写 4 字节对象基址"，而它的源操作数是
`mov ax, seg dseg03` 的 **imm16（2 字节）** ⇒ **多踩了后面 2 字节代码**。三份字节对照
（`0x565A7` = FDPS 的 INT 9 handler `sub_565A7`）：

| 来源 | `66 B8` 之后的 6 字节 |
|---|---|
| 文件 `FDPS.EXE` | `00 00 8E D8 E4 60` |
| IDA（16 位写） | `03 00 8E D8 E4 60` |
| **我们的运行时（4 位写）** | `00 00 **07 00** E4 60` ⇒ `8E D8`（`mov ds,eax`）被改成了 `07 00` |

后果链（单步日志逐条实证，`t ...` 行）：

```
0x565B1: 执行 1 字节 07 (pop es)   esp +4
0x565B2: 执行 00 E4 (add ah,al)    esp 0
0x565B4: 执行 60  (PUSHA)          esp -32   ← 32 字节凭空压栈
... → 0x56602 pop ds 弹到垃圾 → #GP(0xFFFFFFFF) → 崩
```

**修法**（`le.c`）：`type 0x02` 是 16 位选择子 fixup → **写 2 字节**，值 = 本进程的平坦数据选择子
（`__asm mov sel, ds`）；这正是真 DOS/4GW loader 会给 `mov ax,seg X` 填的东西，
之后 `mov ds,eax` 拿到合法平坦选择子，VEH 里连替换都不用触发。
同一条记录就是 §8-37 那次"指针留 0 崩 `mov es,[ebx]`"的记录。

### 18.3 实测判据（`build/host.log`）

```
dos: INT 9 vector := 0x565A7 (game ISR - keys will be delivered there)
isr: handler bytes: FB 52 51 53 50 1E 66 B8 2B 00 8E D8 E4 60 ...   ← 代码完好、选择子=2B
dos: INT 9 queued  scan=0x1C -> handler 0x565A7     （make/break 成对：1C/9C、50/D0）
dos: INT 9 injected scan=0x1C (eip 0x3D25B -> 0x565A7, esp 0x369FC20 -> 0x369FC14)
isr: evt ... eax=0000009C ... bytes=E4 61            ← `in al,60h` 拿到扫描码
（下一次注入 esp 回到 0x369FC20 ⇒ handler 的 pop/iret 完全配平）
```
- 整轮 **0 次 `ACCESS VIOLATION`**、无 `cpu:` 崩溃报告（清理诊断后重跑仍复现）。
- **FD2 未被破坏**：`regress.ps1` **8/8 PASS**（`FD2.TMP = 207360` = 原件同尺寸）——
  FD2 没有 `type 0x02` fixup、也不挂 INT9，两条改动对它都是空操作。

### 18.4 画面证据（`--screenshot --shot-frame=450`，`build/fdps_sbx` 沙箱）

| 图 | 内容 | 直方图 |
|---|---|---|
| `build/fdps_static.png`（按键前） | **标题菜单**：START NEW GAME / LOAD GAME / CONTINUE / EXIT + “FANATaDRAGON 風之聖歌” | 82 色 |
| `build/fdps_menu2.png`（`--autokey=11000:RETURN,RETURN` 之后） | **游戏内场景**：木屋房间、红毯上两个角色、墙上挂钟 | 97 色，与基线逐像素不同 |

⇒ 标题菜单 → START NEW GAME → 进入场景，**按键链路端到端打通**。

### 18.5 下一关口：场景里读完 `FACE.CEL` 后跳飞

```
dos: open 'FACE.CEL' -> 00000340 (0)   （lseek/read/close 正常）
ail: timer fire #400 at +8610 ms
cpu: fault at unreadable EIP=0x1FFFC (read from address 0x43B4)
     eax=04CA9930 ebx=002F9FFE ecx=00000009 edx=00000000 esi=0006A3C1 edi=0006A3BC
```
- `0x43B4` **低于 64 KiB**（Windows 不映射的区域）⇒ 游戏拿一个"本该是线性地址"的值当指针解引用；
  `EIP=0x1FFFC` 说明它是**跳进/执行到**未映射处，不是简单读错。
- 已排除：INT9 注入（注入点 EIP 每次都是 `0x3D25B`、栈配平）、双路按键（已改单路）、fixup 踩字节（已修）。
- **待查**：`FACE.CEL` 解析出来的指针为何是 `0x43B4`/`0x1FFFC` —— 入口是反编译读 FACE.CEL 的那段
  （文件句柄 `00000340`，`int 21 AH=3D/42/3F/3E` 序列在 §18.3 日志末尾），重点看
  **它读的偏移是否来自我们没读对的结构**（如 `AH=42` 的 CX:DX 高位、或 `0x400` 低内存镜像）。
