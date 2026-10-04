# FDPS.EXE（炎龙骑士团外传）逆向测绘地图

> 与 `RE_MAP.md`（FD2）并列的第二张地图。**所有地址为线性地址**，obj0=`0x10000`、
> obj1=`0x60000`、obj2=`0x70000`。凡未经运行验证的结论标注"待确认"。
> 运行侧进度与坑见 `PROGRESS.md` §14/§15。

---

## 0. 分析环境

| 项 | 值 |
|---|---|
| 二进制 | `E:\Games\FDCollection\Game\FDPS\FDPS.EXE`（372 789 B，1998-01-12） |
| IDA 库 | `E:\Games\FDCollection\Game\FDPS\FDPS.EXE.i64`（**2026-10-05 由 ida MCP 无头自动分析后 `save_database` 生成，开库即用**） |
| 分析规模 | 1353 个函数、437 个串、入口 `start` = `0x43008`（与 `re/preflight.py` 的 `entry=0x43008` 一致） |
| 打开方式 | `ida/open_database(path)` → `execute_python` → `save_database`；**不需要手动在 GUI 里加载** |

静态体检（不运行）：`python re/preflight.py <exe>`、`python re/fixup_scan.py <exe>`。

---

## 1. 容器与内存

| 项 | 值 |
|---|---|
| LE 头 | 文件 `0x2A50`，77 页，3 对象 |
| obj0 | `0x10000`，vsize `0x479D0`（游戏 + AIL + CRT + DOS/4GW 尾巴） |
| obj1 | `0x60000`，vsize `0xC3C0`（数据：动画时钟 `dword_69D64`、音频句柄表） |
| obj2 | `0x70000`，vsize `0x54` —— **正好压在宿主低内存镜像上** ⇒ `dos_choose_lowmem()` 把镜像挪到 `0x80000` |
| fixup | 6831 条，含类型 `0x02`（第 7 页，5 字节）；`bad=0 leftover=0` |

---

## 2. 代码分区（obj0 内）

| 区间 | 内容 |
|---|---|
| `0x10000..0x3D487` | 游戏本体 + Watcom CRT（`memset/rand/sprintf/fopen/access` 在 `0x42xxx`，`abs/malloc/free` 在 `0x3D0xx`） |
| **`0x3D488..0x41FFE`** | **Miles AIL 公共入口**（90 个函数，全部带 `AIL_xxx(...)\n` trace 串） |
| `0x42000..0x444xx` | Watcom CRT |
| `0x444xx..0x4Axxx` | **AIL 内部**（定时器 ISR `sub_44516`、核心 `sub_44CBE`、DIG/MDI 驱动装入 `0x45xxx..0x47xxx`） |
| `0x56509..0x566xx` | 被游戏大量调用的底层封装（实模式/DOS 包装，待归类） |

---

## 3. AIL 入口表（`ail.c` 的 `g_entries_fdps[]`，90 条）

**建表手法（可复现，产物见下）**：

1. 扫 `AIL_xxx(` trace 串 → `DataRefsTo(串)` → 所在函数 ⇒ 107 条串 → 90 个函数
   （`re/fdps_ail_trace.csv`）；
2. **全量扫 `call`，目标落在 `0x3D488..0x41FFE`** ⇒ 52 个 call 目标 = 47 个公共入口 + 5 个内部工具
   （`0x3D7B4`/`0x3D7B9` 加解锁、`0x3DA20`/`0x3DA25`/`0x3DBA8`，**不打补丁**）
   （`re/fdps_ail_patchset.csv`）；
3. 只看调用者 `< 0x3D488` 的 ⇒ **游戏真正调的 18 个入口**（`re/fdps_ail_gamecalls.csv`）；
4. 补丁地址两两间距 ≥5 字节检查（90 个地址 0 处冲突）⇒ `re/fdps_ail_table.inc`。

⚠️ **trace 串反查的地址不一定是入口**：IDA 会把相邻函数并成一个，别名（`AIL_start_all_timers`、
`AIL_resume_sequence`、`AIL_release_sequence_handle`…）落在前一个函数体内 ⇒ 真入口以 `call` 目标为准。

### 3.1 游戏调用的 18 个（宿主必须真实现）

| 入口 | 地址 | 调用者 |
|---|---|---|
| `AIL_startup` | `0x3D488` | `sub_30270`（音频总初始化） |
| `AIL_shutdown` | `0x3D622` | `sub_30330`、`sub_30CB0`（spawn 前） |
| `AIL_register_timer` | `0x3DF06` | `sub_30540` |
| `AIL_set_timer_frequency` | `0x3E15A` | `sub_30540` |
| `AIL_start_timer` | `0x3E323` | `sub_30540` |
| `AIL_install_DIG_INI` | `0x3E7D5` | `sub_30270` |
| `AIL_install_MDI_INI` | `0x401BC` | `sub_30270` |
| `AIL_allocate_sample_handle` | `0x3EA1A` | `sub_30270` ×8 |
| `AIL_allocate_sequence_handle` | `0x403ED` | `sub_30270`（句柄存 `dword_69D5C`，**再无其它引用 ⇒ FDPS 不用 XMIDI 音乐**） |
| `AIL_init_sample` | `0x3EC6B` | `sub_142D0`/`sub_303C0`/`sub_30790` |
| `AIL_set_sample_address` | `0x3EDDE` | 同上 |
| `AIL_set_sample_type` | `0x3EE60` | `sub_142D0`/`sub_30790`（WAV 头 → 0/1/2/3 = 声道+位深） |
| `AIL_start_sample` | `0x3EEE2` | 同上 |
| `AIL_stop_sample` | `0x3EF4F` | `sub_30350` |
| `AIL_set_sample_playback_rate` | `0x3F096` | 同上 |
| `AIL_set_sample_volume` | `0x3F10C` | `sub_30790` |
| `AIL_set_sample_loop_count` | `0x3F1F8` | `sub_303C0`/`sub_30790` |
| `AIL_sample_status` | `0x3F26E` | `sub_304D0`/`sub_303C0`/`sub_30790`（**判 `== 4`**） |

其余 72 个接 `host_AIL_unused`，保证没有原版 AIL 代码可执行。

### 3.2 定时器族（本轮的主角）

存档：`re/fdps_ail_AIL_*.c`（包装）、`re/fdps_core_*.c`（核心）、`re/fdps_timer_core_*.c`（ISR）。

| 入口 | 地址 | 核心 |
|---|---|---|
| `AIL_register_timer` | `0x3DF06` | `sub_44CBE`：15 槽，句柄 = 表内字节偏移（0,4,…,56），满回 `-1` |
| `AIL_set_timer_user` | `0x3DFF1` | `sub_44D05` → `user[h]` |
| `AIL_set_timer_period` | `0x3E0E4` | `sub_44E20` → `period[h]`、`counter[h]=0`、重编程 PIT |
| `AIL_set_timer_frequency` | `0x3E15A` | `sub_44E50` = `period = 1000000 / hz` |
| `AIL_start_timer` | `0x3E323` | `sub_44D78`：状态 1→2（`-1` = 全部，同址别名 `start_all_timers`） |
| `AIL_stop_timer` | `0x3E3F2` | `sub_44DCC`：状态 2→1（别名 `stop_all_timers`） |
| `AIL_release_timer_handle` | `0x3E4C1` | `sub_44D2D`：状态 →0 |
| `AIL_release_all_timers` | `0x3E52E` | 清全表 |

- ISR `sub_44516`：`counter += 基准周期`（所有活动定时器的最小 period），`>= period` ⇒ `pending++`，
  然后 `while (pending) { --pending; cb(user); }` ⇒ **停机后追帧**。
- 游戏只用 3 个：`register_timer(sub_30520)` → `set_timer_frequency(h, 0x19)` → `start_timer(h)`
  ⇒ **动画时钟 25 Hz**；回调 `sub_30520 = inc dword_69D64; call rand; ret`（`rand` = `0x42A68`，
  种子是静态变量 `unk_6039C`，无分配 ⇒ 宿主线程并发调用安全）。
- `dword_69D64` 全文 156 处引用（帧延时/动画节拍）；`dword_69D50` = 定时器句柄（4 处，全在 `sub_30540` 内）。

### 3.3 样本状态字（`AIL_sample_status` 的返回值）

`AIL_sample_status` 直接返回句柄 `+4` 的字段（`sub_47000`）；写入点在 DIG 驱动里：

| 值 | 含义 | 证据 |
|---|---|---|
| `1` | 播放中 | `sub_46ED0: mov dword [eax+4], 1` |
| `2` | 循环播放中 | `sub_46F50: mov dword [eax+4], 2` |
| **`4`** | **空闲 / 播完** | `sub_47160: mov [esi+4], 4`；游戏 `for(i<8 && status==4)` 找空闲句柄 |
| `8` | 停止 | `sub_471E0: mov [eax+4], 8` |

---

## 4. 启动 / 退出流程

| 函数 | 作用 |
|---|---|
| `sub_30270(freq)` | AIL 总初始化：`startup` → `install_MDI_INI` → `allocate_sequence_handle` → `install_DIG_INI` → **8× `allocate_sample_handle`** → `sub_30540(freq)` 装定时器。调用者 `0x292DE`、`0x30D98`（都传 `0x19` = 25） |
| `sub_30540(freq)` | `dword_69D50 = register_timer(sub_30520)`；`== -1` 打 `" Timer fail !!!\n"`；`set_timer_frequency(h,freq)`；`start_timer(h)` |
| `sub_303C0(addr,len,loop)` / `sub_30790(...)` | 播数字音效：挑 `status==4` 的句柄 → `init/set_address/set_loop/rate/start`（`sub_30790` 还从 WAV 头算 type 并 `set_sample_type`） |
| `sub_304D0(idx)` | `return status == 4`（"这个槽空闲/播完了"） |
| **`sub_30CB0`** | **退出到 FD.EXE**（存档 `re/fdps_30CB0_spawn.c`）：`AIL_shutdown` → 排空按键 → 淡出 → `sprintf("%sFD.EXE", &unk_643E8)` + 2 个参数 → **`spawnlp(0, path, path, arg1, arg2, 0)`** → 清屏 → `sub_30270(25)` 重新初始化 |

`spawnlp` (`0x43312`) → `spawnvp` → … → **`__dospawn` (`0x55F4B`)，`0x55FDB: mov ah,4Bh`**。
未实现时宿主打印 `dos: UNHANDLED INT21 AH=4B exec <path> (al=.. bx=..)` 并回 `CF=1`
⇒ 游戏直接落到第 4 步重新 init ⇒ **标题 ↔ spawn 死循环**。

---

## 5. 其它已验证事实

- **CD 检测** `sub_3C3A6`：`int386(0x2F, {AX=0x1500})`（MSCDEX 装机检查）→ 再发设备请求；
  后者失败会打印 `DEVICE REQUEST FAILED!!!`（宿主已回 `AL=FFh,BX=0x0210` 过掉第一关，第二关仍失败 —— **待确认是否影响 spawn 流程**）。
- **`access("DISK.NO")`** 是启动第一件事（`AH=43h`，§8-40）。
- 资源：`MISC.VFS`（反复开）、`FDE.SAV`（存档）、`*.WAV`（音效素材：`Chess.wav`/`REST.WAV`/`OpWin.wav`…）、
  `*.DIG`（16 位驱动，宿主拦截）；**没有 `FDMUS.DAT`**，`dword_69D5C`（序列句柄）写后不读 ⇒ 不走 XMIDI。
- 标题循环 `sub_2A280`（存档 `re/fdps_2A280.c`）等 `dword_69D64` 变化 —— 定时器不通时永远停第一帧。

---

## 6. 本目录产物索引

| 文件 | 内容 |
|---|---|
| `fdps_ail_patchset.csv` | 90 个补丁地址 + trace 名 + code ref 数 + ref 种类 |
| `fdps_ail_trace.csv` / `fdps_ail_calls.csv` | trace 串 → 函数 → 调用者全表 |
| `fdps_ail_gamecalls.csv` | 游戏侧（< `0x3D488`）发出的所有跨区调用 |
| `fdps_ail_table.inc` | 直接可粘进 `ail.c` 的 90 条表 |
| `fdps_ail_AIL_*.c`、`fdps_core_*.c`、`fdps_timer_core_*.c`、`fdps_digcore_*.c` | AIL 包装/核心/ISR/驱动 反编译存档 |
| `fdps_*.c`（`30CB0_spawn`、`30270`、`30520`、`30540`、`2A280`、`3C3A6`、`spawnlp`…） | 关键游戏函数反编译存档 |
| `preflight.py` / `fixup_scan.py` | 换游戏前的静态体检（不运行） |

---

## 7. 宿主侧的 EXEC 实现（第 16 轮，`PROGRESS.md` §16）

`sub_30CB0` → `spawnlp` → `__dospawn` → `int 21h AH=4B`。宿主的落地方式：

| 项 | 实现 |
|---|---|
| 子进程 | 再拉一个 `fd2host.exe --exe=<FD.EXE> --gamedir=<父 cwd> --log=<host.<pid>.log> --cmdtail=<尾巴> --exit-after=<剩余秒>`，父进程 `WaitForSingleObject`（`AL=0`） |
| 参数块（`ES:BX` = `0x61528`） | `__dospawn` 写的是 **offset:selector 成对**（选择子基址 0 ⇒ offset 即线性地址）：`+0 env`、`+6 命令尾巴`、`+12/+18 FCB`、`+32 ESP/+36 SS/+38 DS` |
| 命令尾巴 | 指针指向 **`[len][chars][0x0D]`**（不是 C 串！）→ `guest_cmdtail()`；再由 `dos_set_cmdtail()` 写进子进程 **PSP:0x80** |
| 低内存访问 | CRT 解析尾巴用 `mov cl,es:[di-1]`（`emulate_lowmem_access`）+ **`rep scasb`**（`emulate_lowmem_string`，`A4..AF` 全支持）；实测日志 `lowmem string rep AE, 18 left` = 跳过前导空格 |
| `AH=4D` | 回子进程退出码（`__dospawn` 紧接着调它） |

**FD.EXE 的 LE 事实**（实测，`preflight.py` 可复现）：2 对象 24 页，obj0 `0x10000` vsize `0x148A9`、
obj1 `0x30000` vsize `0x3C50`，入口 `start = 0x12280`，fixup 2458 条 `bad=0`。

**当前卡点**：FD.EXE 起来后 `INT10 set video mode 0x13`，紧接着 `open '.\FD1.Aud'`，
文件不存在 → `AH=4Ch code=8`；沙箱里放**空文件**同样 `code=8` ⇒ 内容有格式要求。
`FD1.Vid`/`FD1.Aud` **整个 FDCollection 都没有**（`SETSOUND.EXE` 同目录，怀疑是它生成）。
另：`ail: 'fd.exe' has no AIL table` ⇒ FD.EXE 还需要自己的一张表（用 §3 的手法）。
