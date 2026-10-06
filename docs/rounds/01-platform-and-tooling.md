# 轮次明细：平台层文件服务 + 回归提速
§12（AH=3C/41/40 文件服务补全、关闭鼠标项）、§20（`--exit-when-file` 把回归从 75 s 降到 15 s、
环境性崩溃自动重试）。
---

## 12. 第 12 轮：平台层文件服务补全 + "鼠标"项关闭（2026-10-05）

**这一轮解决什么**：`§7.5` "存档写入" 这条更深路径的**前置平台缺口**。手法是先用 ida MCP 做
**静态覆盖对照**（游戏/CRT 实际会发哪些 `INT 21h` AH vs 宿主已实现哪些），再用沙箱把缺口的
后果**复现成崩溃**，修完用一键脚本回归。产物：`re/int21_ah_used.txt`（AH 清单+所属函数）、
`re/int_sites_all.txt`（obj0 全量 `CD xx` 裸扫）、`re/file_apis.txt`（`sopen`/`unlink` 等反编译）、
`re/file_strings.txt`（`FD2.SAV`/`FD2.TMP` 交叉引用）、`re/sub_19DF7.c`（存档函数）、
`re/sub_10B4E.c`（写 `FD2.TMP` 的那一步）。

### 12.1 缺口与证据

| AH | 谁在用 | 修复前的后果 |
|---|---|---|
| `3C` CREAT | Watcom `sopen()`(0x3D093)：先 `AH=3D` 打开，失败且带 `O_CREAT` 时退回 `AH=3C`，close 后再 open | `fopen("wb")` 打不开**尚不存在**的文件 → CRT 返回 NULL `FILE*` → 游戏 `fwrite(NULL,…)` 解引用 `FILE+0xC` → **AV at 0x377B2，read 0xC** |
| `41` DELETE | CRT `unlink()`(0x46DD0) → `remove()` | 删除恒失败；游戏侧当前无调用点，属兜底 |
| `40` 写 `CX=0` | Watcom `sopen()` 的 `O_TRUNC`（即 `fopen("wb")`） | DOS 语义是"**在当前文件位置截断**"；Windows `WriteFile(…,0,…)` 是**空操作** ⇒ 存档变小时旧存档尾部残留 |

复现（修复前。沙箱 = 数据文件齐全 + 有 `FD2.SAV`、**故意删掉 `FD2.TMP`**，走 continue 路径）：

```
dos: open 'FD2.TMP' -> FFFFFFFF (2)                ← 文件不存在，AH=3D 失败
dos: UNHANDLED INT21 AH=3C (cx=0 dx=500D6 ...)      ← CRT 退回 CREAT，宿主没实现
cpu: unmatched low-memory access: fault=0xC eip=0x377B2 bytes=F6 43 0C 02 75 16 E8 89
cpu: ACCESS VIOLATION at 0x377B2 (Eip=0x377B2) read from address 0xC
```

`F6 43 0C 02` = `test byte [ebx+0xC],2`、`EBX=0` ⇒ 正是 `fwrite` 在解引用空 `FILE*`。
触发点是 **continue 载入存档之后**写 `FD2.TMP` 的那一步（`sub_10B4E`：
`fopen(aFdiconB24,"rb")` … `fopen(aFd2Tmp_0,"wb")` → `fwrite` → `fclose`）。

### 12.2 修了什么

`src/dos.c`：

- `case 0x3C`：`CreateFileA(…, CREATE_ALWAYS, …)`，句柄进 `g_files`（表里新增 `name[64]` 便于日志）；
  失败经新增的 `dos_win_error()` 映射 DOS 错误码（2 找不到 / 5 拒绝 / 6 句柄 / 4 句柄用尽）。
- `case 0x41`：`DeleteFileA` + 同样的错误码映射。
- `case 0x40` 当 `ECX==0`：`SetFilePointer(FILE_CURRENT)` + `SetEndOfFile()`，
  并打印 `dos: truncate '<file>' to N bytes` 供日志核对。

`src/host.c`：`--gamedir=`/`--exe=` 的**等号写法现在也认**（此前静默忽略并回退到 `E:\FD2`）。

### 12.3 同轮关掉的计划项：鼠标 `INT 33h` 不需要做

`§7.3` 原本写着"鼠标 `INT 33h` 接真实状态（宿主尚未实现）"。用 ida MCP 做了四重核实，
结论是**游戏根本不用鼠标**，该项从计划里划掉（宿主 `int33()` stub 保留兜底）：

| 证据 | 结果 |
|---|---|
| obj0 全量**裸字节**扫 `CD xx`（`re/int_sites_all.txt`） | `int 0x33` 只有 **1 处**：`0x469E1`，在 DOS/4GW 的 `int NN; ret` 桩表里（0x46948 起每 3 字节一项，`0x10→0x16→0x33` 步长完全对齐）——**是表项，不是调用** |
| `int386()` 的常量向量（`re/int386_callers.txt`） | 只有 `0x10` / `0x16` / `0x31` |
| `push 0x33` 候选（`re/push33_sites.txt`，3 处） | 全是 `sub_1366A(…,51)` / `sub_34894` 的**标志位索引**，相邻调用传 50/52/53（0x32/0x34/0x35）——设置项编号，不是中断号 |
| 运行期 `host.log` 中断统计 | `int 33` 从未出现（调用数 0） |

> 教训（与 §8-25 同源）：`CD xx` **字节扫描必然是噪声**（本作 obj0 里 0x00..0xFF 每一种向量的
> "字节"都存在），必须先按**指令边界**与**所处区段**（桩表 / CRT / 游戏区）分类才可信。
> 批量产物已落在 `re/int_sites_all.txt`，头部注明了扫描方式。

### 12.4 验证

`port/regress.ps1`（新）：每次从 `E:\FD2` 重建沙箱 → **故意删掉 `FD2.TMP`** → `--autokey` 走
continue 路径 → 对 `host.log` + 文件系统做 8 项断言。修复后 **8/8 PASS**：

```
PASS  AH=3C create issued       PASS  no unhandled INT21
PASS  reopen after create       PASS  no cpu crash
PASS  no unhandled exception    PASS  FD2.TMP created
PASS  FD2.TMP non-empty         PASS  clean end (watchdog/exit)
      FD2.TMP = 207360 bytes (original: 207360)
```

> **第 20 轮更新**：机制已提速 —— `--exit-when-file` 写满即退 + 脚本轮询退出 + 环境性崩溃
> 自动重试，单次 **~15 s**（原 60 s 盲跑 + 15 s 盲睡 = 75 s），见 **§20**。
> `clean end` 断言相应扩为三种干净退出信号：`watchdog fired` / `AH=4Ch terminate` /
> `exit condition met`。

日志关键三行：`dos: open 'FD2.TMP' -> FFFFFFFF (2)` → `dos: create 'FD2.TMP' -> … (dos handle 5)`
→ `dos: open 'FD2.TMP' -> … (0)`，文件尺寸与原版一字不差。

同轮还顺带验到两件事：

- **游戏退出路径已通**（`§7.6` 的一项打勾）：`dos: INT10 set video mode 0x03` →
  `dos: INT 21h AH=4Ch terminate, code=3` → `ail: shutdown` → `dos: game requested exit`。
- **画面无回退**：`build/regress.bmp` 是回归自动生成的画面证据；**第 20 轮起**它由宿主在
  **退出前最后一帧**抓取（原为固定第 900 帧，早退后到不了），实测 36 色、全画面非黑。
- 真实目录的存档**没被测试碰到**：所有回归都在 `build/sandbox` 里跑（`E:\FD2\FD2.SAV` 仍是
  Nov 2025 的原件）。

### 12.5 本轮没做 / 下轮入口

1. **首次保存还没实测**：`fopen("FD2.SAV","wb")` 在 `FD2.SAV` 不存在时同样走 `AH=3C`
   （机制已通、回归只覆盖了 `FD2.TMP`），需要"新游戏 → 存档"走一遍；"存档变小"时的
   `AH=40 CX=0` 截断也还没对拍。
2. `INT 21h AH=49/4A`（free/resize）目前是**空操作返回成功**：分配账本只增不减，
   短跑无害，长跑/反复进出存档时值得改成真释放。
3. 显示层现代化（§7.1，仍是第一优先）、更深路径（战斗/地图）、源码化第一模块，都还没动。
4. 静态确认**无调用点**、暂不实现的 AH：`43` 属性、`4D` 返回码、`4E/4F` 查找、`56` 改名 ——
   除非深层路径里出现新的 `UNHANDLED INT21`（`host.log` 会打印前 40 条）。

---

## 20. 第 20 轮：回归提速（75 s → 15 s）+ 环境性崩溃自动重试（2026-10-05）

**用户反馈**：每次跑游戏启动测试，“后面至少有 10 秒钟没有动，每次都浪费”。

### 20.1 实测浪费在哪（先量化再动手）

用 200 ms 轮询实测一次完整回归的时间线：

```
t=11.7s  FD2.TMP 写满 207360 字节（continue 路径完成，autokey 最后一键 12.5s）
t=60.2s  宿主才退出（--exit-after 看门狗）    ← 静止画面白跑 ~48 s
t=75s    脚本 Start-Sleep($Seconds+15) 才结束 ← 进程已亡又盲等 15 s
```

⇒ 两处浪费：固定 deadline 不知道“路径何时完成”；固定睡眠不知道“进程何时退出”。

### 20.2 宿主：`--exit-when-file=<path>:<minbytes>`（src/host.c）

- **看门狗线程统一两个触发器**：`--exit-after` 硬上限（原行为，日志行不变）与
  **完成触发器**（文件写满 **且** autokey 调度已跑完 → 再缓冲 `EXIT_SETTLE_MS=2000`
  让最后几键落地）→ 走同一条干净退出路径（`dos_terminate_child` + `dos_dump_stats`）。
  新日志行：`host: exit condition reached/met ...`。
- **退出前抓最后一帧**：settle 到点时若 `--screenshot` 还没拍过，把 `--shot-frame` 设为
  `g_frames+1` 再等 250 ms —— 早退也有画面证据（否则永远到不了固定帧号）。
- 文件大小用 **`FindFirstFile`（目录元数据）**轮询，游戏还开着文件写句柄也不会共享冲突；
  路径解析取**最后一个冒号**分隔尺寸，`E:\...` 的盘符冒号不受影响（无数字后缀 = 只要求存在）。
- 看门狗线程创建从 parse 期间挪到 `le_reserve_address_space()` **之后**（所有输入已解析完，
  且不与低址窗预留抢先后）；`--exit-when-file` 与 `--exit-after` 支持空格/等号两种写法。

### 20.3 脚本：轮询退出 + 环境性重试（regress.ps1）

- `Start-Sleep ($Seconds+15)` → **轮询 `$proc.HasExited`**（250 ms 间隔，上限 `Seconds+15`），
  超时才告警停进程；每次跑前删掉旧 `regress.bmp`（只有新鲜截屏才算证据）。
- **重试（最多 3 次）**：断言失败 **且** `host.err` 含 `guest window blocks`（= 加载器抢了低地址窗，
  §8-48 签名 B）才重跑 —— 真实回归没有该签名，首跑失败即报，不会被重试掩盖。
- `clean end` 扩为三种干净退出信号：`watchdog fired` / `AH=4Ch terminate` / `exit condition met`。

### 20.4 插曲：一次偶发启动崩溃的定位（与本轮改动无关，实证链）

改动后第一次回归全灭（宿主 0.3 s 即亡）：`host.log` 有 `cpu: ACCESS VIOLATION at 0x5F4312B0
(Eip=0x5F4312B0) read from address 0xAD000`（EIP 在宿主映像里，像是宿主自己跳飞）。查
`host.err`：`guest window blocks 0x7F00 not reserved`（mask 位 8..14 = `0x90000..0xFFFFF`）+
`cannot commit @0x90000/@0xC0000 (87)` ⇒ **VGA 窗口缺失**，渲染线程转换帧缓冲读到
`0xAD000 = 0xA0000+0xD000`（像素循环中段）时 AV，EIP 自然落在宿主的转换循环里。

- **与本轮改动无关**：早期预留在 `fd2_entry`（进程入口），早于本轮所有新代码；手动重跑
  同参数 **14.7 s 干净退出**。根因是 `le.c` 注释里已记录的已知偶发问题（加载器把 DLL 放进低址窗）。
- 归档为 **§8-48 签名 B**；由 §20.3 的自动重试兕底。

### 20.5 实测结果

```
pwsh -File regress.ps1   →  attempt 1/3 ... ran 15.0 s ... ALL PASS 8/8（连续两次 15 s）
host.log: exit condition reached (file >= 207360 bytes, autokey done) - settling 2000 ms
host.log: exit condition met after 14 s (477 frames drawn)
regress.bmp: 36 色、全画面非黑（退出前最后一帧 = 场景帧）
```

单次回归 **~75 s → ~15 s（省 60 s）**；慢机器/路径未完成时仍由 `--exit-after=60` 兕底，
比原来更稳（完成判据驱动退出，不再赌固定时长）。
