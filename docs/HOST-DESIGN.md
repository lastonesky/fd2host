# 宿主设计（32 位二进制宿主）
地址空间布局硬约束、源文件职责、VEH 四类异常、**DOS/DPMI/BIOS 服务的返回值语义**。
对应旧 `PROGRESS.md` §4。改 `src/le.c` / `src/dos.c` 前必读，尤其是 §4.4 语义表——错一个就跑飞。
---

## 4. 宿主设计

### 4.1 地址空间布局（硬约束，不可随意改）

| 范围 | 用途 | 权限 |
|---|---|---|
| `0x00000000-0x0000FFFF` | **不可映射**（Windows 禁止用户态映射低 64 KiB） | — |
| `0x00010000-0x0004EFFF` | obj0（游戏代码+数据） | RWX |
| `0x00050000-0x00055FFF` | obj1 | RWX |
| `0x00060000-0x00063FFF` | obj2 | RWX |
| `0x00070000-0x0007FFFF` | **低内存镜像**：IVT 映像 / BDA / PSP 区 | RW |
| `0x00080000-0x000FFFFF` | `INT 31h 0x0100` 的实模式可寻址分配区（bump 分配，跳过 0xA0000-0xBFFFF） | RW |
| `0x000A0000-0x000BFFFF` | VGA 帧缓冲（真实映射，游戏直接读写） | RW |

宿主映像自身的两条硬性要求（都踩过坑，见 §8）：
1. **映像体积必须小**（当前 288 KB）。曾因一个 8 MB 静态数组导致 ASLR 只能把映像放进低地址，
   直接压住游戏窗口。
2. **必须保留 ASLR**（`/DYNAMICBASE` 默认开启），并用 `/BASE:0x60000000` 把它推远。
   实测 `/DYNAMICBASE:NO` 会让 **Windows 自己占住低地址窗口**，游戏对象再也映射不进去。

### 4.2 源文件职责

| 文件 | 职责 |
|---|---|
| `src/le.h` `src/le.c` | LE 解析；地址空间预留（`le_reserve_address_space_early`，CRT 之前可调用）；对象映射 + fixup 重定位 |
| `src/dos.h` `src/dos.c` | 平台层：VEH 安装、int 站点改写与分发、DOS/DPMI/BIOS 服务、端口模拟、调色板捕获、低内存镜像、分配账本、单步跟踪 |
| `src/host.c` | 主程序：日志重定向到文件、参数解析、工作目录设置、窗口 + GDI 显示（0xA0000 → 调色板 → 32bpp → `StretchDIBits`）、键盘 → BDA 缓冲、游戏线程、自定义入口 `fd2_entry` |
| `src/letest.c` | 加载器自检（对比 Ghidra 镜像） |
| `src/ail.c` | **AIL 替换层**：把游戏用到的 16 个 `AIL_*` 入口改写为 `jmp` 到宿主实现（音效走 waveOut） |
| `src/xmidi.c` | **XMIDI 回放**：解析 FDMUS.DAT 的 XDIR/CAT/FORM XMID，经 Windows MIDI Mapper 发声 |
| `src/probe*.c` | 可行性探针（低地址可用性、ASLR 行为、映像大小影响） |

### 4.3 VEH 处理的四类异常

```c
EXCEPTION_SINGLE_STEP        → --trace=N 用，记录 EIP 后保持 TF
EXCEPTION_BREAKPOINT         → 命中 int 站点表（int 站点 = (addr-0x10000) 索引，覆盖 obj0）
                               执行 c->Eip = addr + 2（我们只把 CD xx 改写成 CC 90，长度不变）
EXCEPTION_PRIV_INSTRUCTION   → cli/sti/hlt/in/out/lgdt/lidt/lmsw 等，按指令长度跳过
EXCEPTION_ACCESS_VIOLATION   → 三条子规则，按顺序尝试：
   (1) `mov Sreg, r/m16`（跳过段前缀后 opcode==0x8E）：把源值换成宿主平坦选择器
       （c->SegDs，通常 0x2B），重试指令。用于 DOS/4GW 私有选择器 0x24 之类。
   (2) fault < 0x10000 且 EIP 附近（-8..+4）能找到 4 字节等于 fault 值：
       把它改成 fault + 0x70000（指向低内存镜像），重试。用于绝对寻址的 BDA/PSP 访问。
   (3) fault < 0x10000：`emulate_lowmem_access` 解码 `mov r, r/m`（8A/8B/0FB6/0FB7）
       并针对低内存模拟执行、跳过指令。**仅支持 mod!=3、非 SIB 寻址**——已知不全，见 §6。
```

### 4.4 已实现的服务及其**返回值语义**（错一个就跑飞）

| 调用 | 实现要点 |
|---|---|
| `INT 21h AH=30h` | 返回 `0x42431606`（扩展器签名 `'BC'` + DOS 6.22）。**这是关键**：返回 `'XD'` 会走另一条分支并走进空环境指针；返回普通 DOS 版本号则会走"探测扩展器"分支，那条分支才读 `PSP:0x2C` |
| `INT 21h AH=0xFF` | DOS/4GW 私有探测，**必须返回 AL≠0**（否则启动代码按"未知扩展器"处理，环境指针为 0） |
| `INT 21h AH=2Ch` | 时/分/秒/百分秒 → CX/DX，**并把 AL 清零**（DOS 行为；不清零会让调用方走错分支——已实测） |
| `INT 21h AH=48h` | 返回**线性地址**（不是段值），调用方直接解引用；AIL 一次要 512 KiB |
| `INT 21h AH=3Dh` | `*.DIG` / `*.MDI` 一律报"文件不存在"（16 位驱动无法执行，见 §8） |
| `INT 21h AH=3Ch` | **创建/截断文件**（`CreateFileA` + `CREATE_ALWAYS`）。`fopen("wb")` 对不存在的文件必走这里，缺了就拿到 NULL `FILE*` 崩溃（§12） |
| `INT 21h AH=41h` | **删除文件**（`DeleteFileA`）。游戏侧当前无调用点，为 CRT `unlink()` 兜底 |
| `INT 21h AH=40h` | `CX=0` ⇒ **在当前文件位置截断**（DOS 语义，Watcom 用它实现 `O_TRUNC`）。Windows `WriteFile(...,0,...)` 是空操作，必须显式 `SetEndOfFile` |
| `INT 31h AX=0100h` | 在 `0x80000..0xFFFFF` 真实分配；`AX=段值`、`DX=宿主平坦选择器` |
| `INT 31h AX=0501h` | 按 DPMI 语义：`BX:CX` 入参是字节数，出参 `BX:CX` 是线性地址（此前返回假值 ⇒ 游戏拿到野指针） |
| 端口 `0x3C8/0x3C9` | 捕获成调色板表（`dos_palette`），供显示层转 RGB |
| 端口 `0x3C0/0x3C2/0x3C4/0x3CE/0x3D4/0x3D5` | 忽略（不需要，帧缓冲是真实内存） |
| 端口 `0x3DA` | 返回 0x09（显示使能） |
| BDA `0x46C` | 后台线程按 18.2 Hz 递增，供游戏计时 |
| BDA `0x41A/0x41C` + `0x41E` | 键盘环形缓冲；`WM_KEYDOWN/UP` 经 `MapVirtualKeyA` 转扫描码后写入 |
| `INT 16h` AH=0/1/2/0x10/0x11 | 从 BDA 缓冲取键（`kbd_fetch()`）：返回 `AX = (扫描码<<8)\|ascii`；扩展键的 ascii 字节为 `0xE0`。**菜单导航依赖它**（见 §8-22） |
