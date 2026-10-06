# 轮次明细：跨平台第 2 刀 —— `dos.c` 过河（故障分发 + 文件服务 + 低内存镜像）
§45 把"执行游戏代码的那半边"（异常分发、INT 21h 文件服务、0x70000 低内存镜像）从 Win32
换成 platform.h 缝 + 平台故障入口，并给出**两平台同一套 49 项断言**的自检 `doscheck`
以及 Linux 故障模型的实测探针（`faultprobe32/64`）。Windows 回归 8/8、跨版本同 tick 抓帧 **0 px**。
---

## 45.1 起点 / 门槛（PROGRESS 下一步第 7 条、`rounds/13` §43.5 第 2 项）

> **下一刀 = `dos.c`（异常 + 文件服务）**，之后入口层，最后 `-m32` 跑真游戏。

第 1 刀（§43）之后 `le.c` 已零 Win32，但 `dos.c` 还攥着五样东西：
`AddVectoredExceptionHandler`、`ReadFile/WriteFile/SetFilePointer`、`VirtualAlloc`、
`CreateThread/Sleep`、`CreateProcess(AH=4B)`。本轮全部落地，外加一个新问题：
**Linux 的故障语义与 Windows 不同，必须先实测再写**（实证优先）。

## 45.2 先测后写：两平台故障模型实测

Windows 侧的既有证据是 `src/probe4.c`（round §12 时代测的）。Linux 侧本轮新写两个探针：

| 探针 | 形态 | 用途 |
|---|---|---|
| `src/faultprobe32.c` | **freestanding `-m32 -nostdlib`**（裸 `int 0x80` 系统调用 + 手写 sigaction，**不需要 gcc-multilib**） | i386 **compat 模式** = 游戏真正运行的模式 |
| `src/faultprobe64.c` | glibc x86-64 | 编译/冒烟架构（doscheck 的64位侧） |

`make -f Makefile.linux` 两个都编。**i386 compat 实测表**（与 Windows probe4 对照）：

| 事件 | Windows（probe4） | Linux i386（faultprobe32） | 移植处理 |
|---|---|---|---|
| `int NN` | `ACCESS_VIOLATION`，EIP **停在** `CD` 上 | `SIGSEGV si_code=SI_KERNEL(128)`，`si_addr=NULL`，EIP **同样停在** `CD` 上 | 同一条规则：解码 EIP 处字节 |
| `in/out/cli/sti/hlt/lgdt` | `PRIV_INSTRUCTION` | 同上 `SIGSEGV/SI_KERNEL`（**与 int 无法用信号区分**） | core 按字节查特权表 |
| `mov ds, <非法选择器>` | `ACCESS_VIOLATION` | 同上 `SIGSEGV/SI_KERNEL` | 进段替换规则（`8E`） |
| `int 3` / `CD 03` | `BREAKPOINT`，EIP **停在**指令上 | `SIGTRAP si_code=128`，EIP **已越过**指令 | 包装层 `break_fix_eip` 回退 1/2 字节 |
| TF 单步 | `SINGLE_STEP` | `SIGTRAP si_code=TRAP_TRACE(2)`，EIP=下一条 | 同 |
| 页错误 | `AV` + `ExceptionInformation[1]` | `si_code=1(MAPERR)/2(ACCERR)` + `si_addr=cr2` | `has_addr=1` 才走低内存改写 |
| `ud2` / `div0` | `ILLEGAL_INSTRUCTION` / `INT_DIVIDE_BY_ZERO` | `SIGILL(2)` / `SIGFPE(1)` | `FD2_FAULT_OTHER` |
| `int 0x80` | （会进分发） | **不炸——真 syscall** | 已知分歧，见 §45.7 |
| ucontext 布局 | — | eip=`uc+76`、eflags=`+84`、ds=`+32`、err=`+72`（探针**逐字打印原始字**验证，glibc `mcontext_t` 即内核 `sigcontext`） | `dos_fault_posix.c` 直接用 glibc 字段 |

**两条 Linux 事实决定了设计**（已写成 `docs/PITFALLS.md` §8-61）：

1. `SI_KERNEL` **不带故障地址**（`si_addr=NULL`）——不能拿它当"访问地址 0"用，
   否则"`fault < 0x10000` 就改写低内存操作数"那条规则会把代码里无关的 `00 00 00 00`
   立即数改坏。⇒ `si_code∈{MAPERR,ACCERR}` 才 `has_addr=1`。
2. int / 特权指令 / 段选择器三种身份**只能靠解码指令字节**区分（`CD` → 特权表 → `8E` → 其余）。

## 45.3 设计：把"故障是什么"与"故障怎么来"切开

新增 `src/dos_fault.h`（模块内契约）：

```c
fd2_fault { kind: STEP/BREAK/ACCESS/PRIV/OTHER, has_addr, addr, access(0r/1w/8fetch),
            code, info }           /* code/info = 平台原始值，只进事件环 */
fd2_action dos_fault_core(dos_ctx*, const fd2_fault*, int *exit_code)
                     /* CONTINUE / SEARCH(不是我们的) / EXIT(n) */
void       dos_fault_install(void)
```

- **`dos.c`** = 可移植的故障核心（原 `fd2_veh` 函数体逐段搬入，分支条件从
  `er->ExceptionCode == …` 换成 `f->kind == …`，Windows 分支**逐条等价**），
  加上全部 INT 服务；**零 `windows.h`**（`grep -i windows src/dos.c` → 只剩注释）。
- **`src/dos_fault_win.c`** = VEH：`CONTEXT ⇄ dos_ctx` 拷贝 + 异常码 → `FD2_FAULT_*` 映射；
  **`src/dos_fault_posix.c`** = `sigaction(SA_ONSTACK|SA_SIGINFO)` ×5（SEGV/ILL/TRAP/BUS/FPE）
  + `ucontext ⇄ dos_ctx` + 信号 → `FD2_FAULT_*` 映射 + int3 EIP 回退。
- **`dos.h`** 去掉 `#include <windows.h>`，新增便携寄存器帧 **`dos_ctx`**
  （字段名沿用 Win32 `CONTEXT` 拼写，`int21()` 等服务代码一字未改地在两边跑）
  与自检入口 **`dos_service(vec, ctx)`**（`dispatch_swint` 拆出来的服务开关）。
- 处理器入口跑在 **sigaltstack（64 KiB）** 上：故障时 ESP 在游戏栈里，sigframe 和
  handler 深度都不能落在 guest 内存上。
- `SEARCH`（"不是我们的"，EIP ≥1MB 且不在分配块）在 POSIX = 恢复 `SIG_DFL` 再返回 ⇒
  内核重执行指令 → 默认动作杀进程，等价于 Windows 的 `CONTINUE_SEARCH` 走未处理路径。

### platform.h 第 2 切片（`dos.c` 用到的 OS 原语）

```
文件   plat_console_file(fd) / plat_file_open/create/close/read_at/write_at/
       write_seq/truncate/size/delete/attrs/set_attrs + plat_error_to_dos
时间   plat_local_time（AH=2A/2C）
线程   plat_thread(分离) / plat_sleep_ms / plat_thread_id / plat_exit
内存   plat_readable（VirtualQuery ⇒ process_vm_readv(self)，首末字节探测，
          与原区域查询同覆盖面且每调一次只花 1-2 µs —— 每个被拦截的 int 都要过它）
状态   plat_mem_status（INT31 0500）
进程   plat_exec_child(AH=4B) / plat_child_present / plat_child_kill
```

**文件位置由调用方持有**：`dos_file` 多一个 `pos` 字段，读写全部 `*_at(off)`（POSIX =
`pread/pwrite`，Windows = `SetFilePointerEx+Read/WriteFile`），平台层不再有游标 ——
两边跑**同一套位置算术**，且更贴近 DOS（位置是句柄状态）。三条 DOS 语义显式保住：
`AH=40 CX=0` 截断（§8-31）、`CX:DX` 是一个无符号 32 位偏移（旧代码把 CX 当高位，落 171 GB）、
负 SEEK_CUR/END 按旧 `(LONG)` 行为用带符号加法（结果 <0 → `AX=6`）。

低内存镜像（0x70000）本身在第 1 刀后已走 `le_commit_range → plat_*`，本轮只需把
`files_init`/BIOS tick 线程（`plat_thread`+`plat_sleep_ms`）换缝即可，`dos_init_lowmem`
逻辑零改动。

## 45.4 `doscheck`：同一套 49 条断言跑两个平台

`src/doscheck.c`（`build.ps1 -Target doscheck` / `make -f Makefile.linux doscheck-linux`），
四层：

1. **低内存**：BDA 模式/列数/键盘环、IVT[0x0B] 段值、`PSP:0x80` 命令尾巴、
   BIOS tick 线程 200 ms 内推进 ≥2；
2. **平台缝**：`plat_reserve` 定址 → 不可读 → commit → 可读 → release → 不可读
   （这一项**当场抓到 §8-62 的 `MEM_RELEASE` 静默失败**）、`plat_readable(NULL)` 等；
3. **INT 21h 文件服务**（经 `dos_service` 直驱，不经 `int`）：建/写/读/seek（含
   `002A:1CF3` 大偏移）/EOF 读/中段覆盖/`CX=0` 截断后 size==8/属性增删/关句柄再开/
   缺失文件 `AX=2`/删除两次/控制台写 … 共 31 项；
4. **故障分发**：低页 `0x30000800` 上一段**手写字节码桩**执行真 `int 0x21`
   （AH=30 拿 `0x42431606` 签名、AH=62 拿 PSP 段、**CF 经寄存器帧回写**）——
   Windows 走 VEH、Linux 走 sigaction，同一段桩字节两边通用（`pushf/pop/ret` 编码在
   32 位与长模式下同义）。

**为什么桩要放进 `0x30000000` 固定低页**：`dos_ctx` 是 32 位帧，64 位主机上
桩/字符串/缓冲区的栈地址一进寄存器就被截断（§8-63，抓到过 `open` 收到 `0x8198C01C`
的实锤）——与游戏同一规则：**穿过 `dos_ctx` 的指针必须 < 4 GiB**。

## 45.5 判据（全过）

| 项 | Windows（MSVC 32-bit） | Linux（gcc14，WSL Debian13） |
|---|---|---|
| 构建 | `build.ps1 -Target all`：**无新增警告**（仅既有的 vendor/sokol C4819 与 `render_sokol.c` C4474，均非本轮引入） | `make -f Makefile.linux` **零警告** |
| `doscheck` | **49/49 PASS**，exit 0 | **49/49 PASS**，10 连跑全绿 exit 0 |
| `letest` | `reference check OK, exact match`；obj0 `0x2C48FADEDD2A735B` obj1 `0x3C879E6011769348` obj2 `0xB45FE50C6829E13B` | **三个哈希逐字相同**，entry `0x3CCB4` 同 |
| `platprobe` | exit 0 | exit 0 |
| 回归 | `regress.ps1` **ALL PASS（8/8）**，`FD2.TMP=207360` | —（宿主未移植，见 §45.7） |
| 显示 A/B | 旧版 vs 新版同 tick **0 / 64000 px** | — |
| 故障模型 | `probe4.c`（既有） | `faultprobe32` 实测表（§45.2）+ doscheck 第 4 层 |

**A/B 方法（本轮踩了 §8-53 才走对）**：`--shot-tick=600` 裸抓的**基线就是 22%**
（intro 转场，同二进制连跑两次 22.23%）——这种点不能比。改用回归配方
`--autokey=5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN` 进菜单后仍取 tick600：
**同版本基线 0 px**，然后 `git stash` → 旧构建 → 抓 → `stash pop` → 新构建 → 抓 →
`framediff` = **0 px**。教训：**先量基线，基线不为 0 的取样点直接弃用**。

## 45.6 本轮踩到的坑（按发现顺序）

1. **`dos_ctx` 32 位 vs 64 位主机**：doscheck 首跑即崩（截断指针进 guest 服务）→ 固定低页 + 字节码桩（§8-63）。
2. **上下文回写截断**：MERGE64 之前，第一次服务完 `int`，RSP 丢高 32 位 → 下一条 `push` 砸未映射页（§8-63）。
3. **桩内 `xor ebx,ebx` 写在 `push rbx` 之前**：保存的是 0，`ret` 后宿主的 callee-saved rbx
   （编译器正拿它当桩地址）被清零 → 第二次 `call *%rbx` **EIP=0**。定位靠 `objdump` 对崩溃现场
   （`eax=g_scratch、esp=FC08` 逐项吻合）。**方法教训**：带 printf 的版本怎么跑都过——
   printf 改变了寄存器分配把 bug 掩了；去掉打印后 **10 连跑 10 崩**才钉死。
   判据从此固定：**桩/内联汇编类修复必须"去打印 + N 连跑"**。
4. **`file_alloc` 没初始化新加的 `pos`**：复用句柄位时读到上一个文件的旧位置
   （旧 Windows 行为"重开必从 BOF"是 OS 白送的，自己管位置后要自己置 0）。
5. **`plat_release` 的 `MEM_RELEASE` 传了 `len`** → 静默失败（§8-62，round43 遗留）。
6. **源码注释写中文触发 MSVC C4819**（CP936 表示不出 UTF-8 字节）：`platform.h` 里两个
   "待确认"换成英文即消；源码注释保持 ASCII+`§`（`§` 恰好是合法 GBK 双字节，既有文件都这么活的）。
7. **`echo $?` 经 wsl 层取不到真退出码**（实测被上层提前替换成 0，`/bin/false` 也报 0）：
   取退出码用 `python3 -c "subprocess.run(...).returncode"` 或落文件再读（本轮用前者）。
8. **tick600 转场基线 22%**（§45.5）。

## 45.7 本轮没做（下一步的门槛）

1. **入口层**（下一轮）：`MapVirtualKeyA/ToAscii` → X11 `XLookupString`/keysym 表；
   截图（`winshot.c` 是 `PrintWindow`）→ sokol `sapp` 帧缓冲读取。见 `rounds/13` §43.5 第 3 项。
2. **`-m32`**（最后一刀）：游戏是 32 位 x86，64 位进程跑不了 ⇒ `gcc-multilib` +
   32 位 X11/ALSA 多架构包；届时用 `faultprobe32` 复核 §45.2 表（本轮已按 compat 模式测，
   但"游戏真跑时"的现场要再验一遍）。
3. **`AH=4B` POSIX 实现待确认**：`fork+execv` 已写（`platform_posix.c`），FD2 **静态证明不调用**
   （`re/int21_ah_used.txt` 无 `0x4B`），FDPS 冻结 ⇒ 无运行验证。
4. **INT31 0500 的 ECX 语义待确认**：Windows 给 `dwAvailVirtual`，POSIX 没有便宜等价物，
   暂用总内存顶替（FD2 未见调用）。
5. **`int 0x80/0x81/0x82` 分歧**：Linux 上 `int 0x80` 是真 syscall。`re/int_sites_all.txt`
   里 `0x46AC8..0x46ACE` 三条连续（步长 3，像数据表）—— `-m32` 轮要用 `--trace` 实测游戏不执行它。
6. **Linux 二次故障行为不同**：handler 里再炸（`SA_NODEFER` 未设）→ 内核默认动作直接杀；
   Windows VEH 会重入。现有代码所有 guest 内存读都过 `guest_readable`，理论触不到；
   真触到时 Linux 的死法更"干脆"（拿不到第二次报告）。
7. `host.c`/AIL/入口层线程与时间（`GetTickCount/FindFirstFile/Interlocked…`）仍在 Windows 缝里，
   归入口层轮。
