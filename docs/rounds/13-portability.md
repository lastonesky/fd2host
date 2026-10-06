# 轮次明细：跨平台第 1 步 —— `platform.h` 与加载器过河
§43 抽出 OS 适配层（先切内存这一刀），把**核心加载器 `le.c` 送去 Linux 逐字节验证**，
并给出全仓 Win32 依赖面的测绘（后续每一轮照这张表推进）。
---

## 43.1 起点：`PROGRESS` 下一步第 7 条 / `BACKEND` §13.6 第 4 步

> "抽 OS 适配层（内存/线程/文件/异常），Win32 实现进 `platform_win32.c`，POSIX 实现进
> `platform_posix.c`；先 Linux x86-64。"

**不能一次抽完**（那样改动面太大、没法验证），所以本轮只切**内存**这一刀，但把整张依赖面
量出来，后面的轮次照表推进。

### Win32 依赖面实测（`grep` 全 `src/*.c`）

| 文件 | 依赖 | 归属轮次 |
|---|---|---|
| `le.c` | `VirtualAlloc`/`VirtualQuery`/`GetLastError`/`MAX_PATH`/`__asm` | **本轮已清零** |
| `dos.c` | `AddVectoredExceptionHandler`、`ReadFile/WriteFile/SetFilePointer`、`VirtualAlloc`、`CreateThread`、`ExitProcess` | 下一刀：**异常 + 文件服务** |
| `host.c` | `CreateThread`、`GetTickCount`、`Sleep`、`FindFirstFile`、`WriteFile`、`_strdup/_stricmp/_strnicmp` | 线程/时间/CRT 名字 |
| `ail.c` `xmidi.c` `synth.c` `keylog.c` `audio_sokol.c` | `CreateThread`、`WaitForSingleObject`、`GetTickCount`、`Sleep`、`Interlocked*`、`CRITICAL_SECTION` | 线程/时间/原子 |
| `main_sokol.c` | `GetKeyboardState`/`ToAscii`/`MapVirtualKeyA`（ASCII 生成） | 入口层：键码翻译 |
| `main_win32.c` `render_gdi.c` `winshot.c` | 窗口/GDI/`PrintWindow` | **Windows 专用后端**，不迁移 |
| `repl.c` 与各 `*check.c` | `VirtualProtect`（写 5 字节 `jmp`） | POSIX = `mprotect` |
| `entry.c` | `ExitProcess` | 一行 |
| `audio_sokol.c` | sokol_audio（Win=WASAPI / **Linux=ALSA**） | **已跨平台**（§41 抽的接口正好用上） |
| `render_sokol.c` | sokol_gfx（Win=D3D11 / Linux=GL） | **已跨平台** |

结论：**渲染与音频已经天然跨平台**（§41/§36 的接口化在这里兑现了），剩下的是三块硬骨头：
`dos.c`（VEH + 文件服务）、入口层（键码/窗口）、以及**执行 32 位游戏代码本身**（见 §43.5）。

## 43.2 设计：`platform.h` 的契约

```c
plat_reserve(addr, len)        /* 占住地址、不可访问 */
plat_commit(addr, len, prot)   /* 让它可访问：空闲则先占再开权限，已有则开权限；
                                  是*别人*的映射就失败（与 Windows 一致） */
plat_release(addr, len)
plat_query(addr, &region)      /* 这块地址现在归谁：base/size/is_free/prot */
plat_error() / plat_error_text()  /* GetLastError / errno */
plat_image_base()  plat_describe()  plat_data_selector()
```

**四条语义差异**（都写进了 `platform.h` 与 `platform_posix.c` 的注释，因为它们正是移植会踩的地方）：

| 点 | Windows | Linux | 我们的选择 |
|---|---|---|---|
| 占住地址 | `MEM_RESERVE` | 只有 `mmap`（无 reserve/commit 之分） | `PROT_NONE` 映射当"占位"，之后 `mprotect` 当"commit" |
| 覆盖别人 | `VirtualAlloc` **失败** | `MAP_FIXED` **直接覆盖** | `MAP_FIXED_NOREPLACE`（失败而不是覆盖） |
| 低地址 | 加载器/DLL 抢 `0x10000`（§8-48 老坑） | `mmap_min_addr=65536`，正好等于 `0x10000`；进程自身映射在 `0x55…/0x7f…`，低窗没人抢 | 直接可用，**§8-48 那类竞态在 Linux 上不存在** |
| 区域粒度 | 每 64 KiB 一块（`MEM_COMMIT` 单区校验 → 487） | `/proc/self/maps` 把同权限相邻映射**合并** | 调用方仍按区域分块（`le_commit_range`），Linux 只是块更大 |
| 空闲地址的返回值 | 曾返回 0（"查到了一个空闲区"） | 返回 -1（"没人占"） | **统一为 -1**，`is_free=1` 是有效答案，调用方只看 `is_free` |

**权限编码**：`PLAT_PROT_R/W/X` 是三个**可组合的 bit**，`RWX = R|W|X`；
POSIX 侧 1:1 映射到 `PROT_*`，Win32 侧翻译成 `PAGE_*` 组合（它没有"只写不可读"）。

## 43.3 迁移范围

| 改动 | 说明 |
|---|---|
| 新增 `src/platform.h` + `platform_win32.c` + `platform_posix.c` | 上面那套契约 |
| `src/le.c` | **零 Win32 依赖**（`grep PAGE_|VirtualAlloc|GetLastError|windows.h` → 空）；`map_at`/`le_commit_range` 改用 `plat_*`；`0x02` fixup 的 `__asm { mov sel, ds }` → `plat_data_selector()`；失败报告改用 `plat_image_base()` + `plat_describe()`（保留 §8-48 引用的 `type=`/`prot=`/`mapping:` 字样） |
| `src/dos.c` | 唯一一处 `le_commit_range(..., PAGE_READWRITE, ...)` → `PLAT_PROT_RW` |
| `src/letest.c` | 去掉 `windows.h`；新增 **FNV-1a 校验值**与**参考目录参数**；**差异分类 + 非零退出码** |
| 新增 `src/platprobe.c` | 跨平台平台自检（`probe*.c` 的后继）：预留窗口 → 分块 commit → 触碰 → 整窗 |
| `build.ps1` | 新目标 `platprobe`；链 `le.c` 的目标自动补 `platform_win32.c` |
| `Makefile.linux` | POSIX 构建（`letest-linux`、`platprobe-linux`） |

## 43.4 判据

| 项 | Windows (MSVC 32-bit) | Linux (gcc14, WSL Debian13) |
|---|---|---|
| 构建 | 三个目标**零警告**（`platprobe`/`letest`/`fd2host`） | `make -f Makefile.linux` **零警告** |
| `platprobe` | 对象窗 `prot=0x7`、分块 commit ok、整窗 ok，exit **0** | 同（`rwxp 10000..100000`），exit **0** |
| `letest` | `reference check OK`，exit **0** | 同，exit **0** |
| **跨平台一致** | `obj0 fnv1a=0xCB737A9DC0F6653E`<br>`obj1 fnv1a=0x3C879E6011769348`<br>`obj2 fnv1a=0xB45FE50C6829E13B` | **三个哈希完全相同**；`fixups applied=7937, cross-page skipped=22, bad=0`、`entry=0x3CCB4` 也全同 |
| 回归 | `regress.ps1` **ALL PASS（8/8）**、`FD2.TMP=207360` | —（宿主尚未移植，见 §43.5） |

> **参考镜像的现状**（`build/object*.bin` 被 `.gitignore` 忽略，本机没有）：本轮由 **IDA 独立映射**
> 导出一份做逐字节对照，差异**全部可解释**，因此 `letest` 新增了差异分类：
>
> | 对象 | 差异 | 类别 |
> |---|---|---|
> | obj0 | **11/257833** 字节 | 全部落在**页边界**（`page_off ∈ {0,1,0xFFF}`）= **跨页 fixup**：我们按 `PITFALLS §8-11` 故意跳过（写它会踩掉下一页开头的代码），IDA 则解析 |
> | obj1 | **5808/22192** 字节 | 全部在 `page_count*0x1000` **之外**（`+0x4000` 起）= **BSS 尾**：我们映射页数、窗口其余是零填充；IDA 按 `vsize` 映射并填 `0xFF` |
> | obj2 | **0/13522** | 完全一致 |
>
> 分类外的差异 → `letest` 打印 `FAIL` 并返回 **1**。Ghidra 镜像仍是原判据（`AGENTS.md` §6），
> 桥接可用时应回归它；**跨平台哈希是不依赖外部文件的第二判据**。

## 43.5 本轮**没做**的（下一步的门槛）

1. **执行游戏代码需要 32 位**：游戏是 32 位 x86，64 位进程里跑不了 ⇒ Linux 侧最终要
   `gcc -m32`（`gcc-multilib` + 32 位 `libx11/alsa` 多架构包）。本轮 `letest`/`platprobe`
   只做内存与字节，不需要执行 guest 代码，所以 64 位即可验证。
2. **`dos.c`**：VEH（POSIX = `sigaction` + `SIGSEGV`/`SIGILL`，且语义不同——Linux 没有
   "修完指令长度再继续"的 VEH 等价物，要用 `ucontext_t->uc_mcontext.gregs[REG_EIP]`）、
   文件服务（`ReadFile/WriteFile/SetFilePointer` → `pread/pwrite/lseek`）、低内存镜像
   （`0x70000` 那 64 KiB，`mmap` 即可）、`INT 21h` 语义表不变。
3. **入口层**：`MapVirtualKeyA`/`ToAscii` 的 ASCII 生成 → X11 `XLookupString`/keysym 表；
   截图（`winshot.c` 是 `PrintWindow`）→ sokol 的 `sapp` 帧缓冲读取。
4. **`plat_data_selector()` 在 Linux64 返回 `0x0`**（长模式下 DS 就是 0），而 Windows 是 `0x2B`：
   FD2 **没有** `0x02` fixup（`PITFALLS §8-49`：只有 FDPS 有一条），所以本轮哈希不受影响；
   **FDPS 在 Linux 上的这条 fixup 属于"待确认"**。

### 43.6 下轮入口

按 §43.5 顺序：**第 2 刀 = `dos.c`（异常 + 文件服务）**，之后入口层，最后 `-m32` 跑真游戏。
`Makefile.linux` 是入口（`make -f Makefile.linux`），新增目标记得两边都挂。
