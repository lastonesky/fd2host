# FD2 → Windows 原生移植 — 进度存档 / 交接文档

> 这份文档的目的：任何模型或人接手都能从这里继续，不必重新逆向。
> 所有结论都经过实证（Ghidra 镜像逐字节对比 / host.log 运行日志），未证实的会标注"待确认"。

---

## 0. 一句话现状

32 位宿主 `port/build/fd2host.exe` 已能把 DOS/4GW 的 `FD2.EXE` 加载进 Win32 进程、接管它全部的
`int`/端口/低地址访问，并让**原始 x86 游戏代码原生执行**：设置 320×200×256 视频模式、按 DAT
偏移表加载资源、**持续渲染开场动画**，而且**有声音** —— 数字音效走 WinMM waveOut、音乐走
Windows MIDI（Miles AIL 的 16 个入口已被宿主实现替换，见 §11）。

实测（2026-10-05）：**"进入游戏"这条路已经走通** —— 用 `--autokey` 自动按键走完
"片头 → 标题菜单 → continue"，游戏能加载存档并进入剧情画面（王座厅 + 对话框，见 `--screenshot`），
连续运行无异常退出；**"continue 就退出"的真凶已定位并修复**（§11.5：int 站点扫描改坏了
`call sub_34894` 的位移）。音乐的两处根本错误也已修正（§11：delta 是"累加"、Note-On 自带音长），
菜单曲从错误的 316 秒变成正确的 **74.5 秒（137 拍 @ 112 BPM）**。
音效样本（8 位无符号单声道 @ 11025 Hz）与音乐（XMIDI）均已解析并播放。

- §6 的两个卡点（RLE 解压写飞 `0xC0005`、`__ExpandDGROUP` 越界写 `0x48FFFF8`）**均已定位并修复**，
  根因都是宿主 INT 21h 的入参/返回值语义错误（见 §8 第 15、16 条）。
- 显示层的色偏（红蓝互换 + 亮度只有 25%）也已修复（见 §8 第 17 条）。
- **第 12 轮（2026-10-05）：平台层文件服务补全** —— 补上 `INT 21h AH=3C`（创建）、`AH=41`（删除）
  与 `AH=40 CX=0` 的"DOS 截断"语义。fresh install 缺 `FD2.TMP` 时游戏从**必崩**变成正常创建，
  回归脚本 `port/regress.ps1` **8/8 PASS**；同轮还用 ida MCP 静态证明"**游戏不用鼠标**"，
  把 `§7.3` 的 `INT 33h` 项从计划里划掉了。详见 **§12**。
- **第 13 轮（2026-10-05）：显示/跨平台选型定案 sokol + 第 1 步接口抽取完成**。
  实测三方：SDL2 = 1.28 MB DLL 且默认 **D3D9**、SDL3 = 2.25 MB DLL 默认 D3D11、
  **sokol = 0 DLL / exe +146 KB / Win 上直接 D3D11** ⇒ 选定 sokol（需求重定义为"替换 GDI"）。
  第 1 步已落地：`render.h`+`render_gdi.c`+`host.h`+`main_win32.c` 拆出，GDI 变成第一个后端，
  `regress.ps1` **8/8 PASS**、帧 900 画面与重构前一致。详见 **§13**。

⇒ 路线 C 的 POC 目标"**窗口中看到游戏画面**"**已达成**。下一步见 §7。
逆向侧：IDA Pro 9.5 + ida MCP 环境已建好，测绘结果在 `port/re/RE_MAP.md`（见 §10）。

---

## 1. 任务与路线

- **目标**：`E:\FD2\FD2.EXE`（DOS/4GW + Borland/Watcom + Miles AIL 的 32 位保护模式游戏）
  在 Windows 上**原生**运行。**不使用** DOSBox、不模拟 DOS、不模拟实模式 CPU。
- **路线 C（用户已选定）**：先做 32 位"二进制宿主"——保留原始 x86 游戏逻辑，把平台层
  （图形/声音/输入/文件）换成现代 Windows 实现；之后再逐步把机器码函数替换成反编译出的 C 源码，
  最终产出可编译 x86-64 的引擎。
- **当前阶段**：路线 C 的可行性 POC（目标：窗口中看到游戏画面）。

---

## 2. 环境、工具链、复现命令

| 项 | 值 |
|---|---|
| 编译器 | MSVC 14.51（VS 18 Enterprise），`C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat` |
| 目标 | 32 位 x86（`x86:LE:32`），Windows SDK 10.0.26100 |
| 反汇编 | Ghidra 12.0.4 @ `E:\Dev\ghidra_12.0.4_PUBLIC`；ghidra-mcp @ `E:\Dev\ghidra-mcp` |
| 游戏目录 | `E:\FD2`（宿主必须以此为工作目录） |

```powershell
# 构建全部/单个目标
pwsh -File E:\FD2\port\build.ps1 -Target fd2host
pwsh -File E:\FD2\port\build.ps1 -Target letest

# 运行（宿主是 WINDOWS 子系统，不弹控制台；日志写入 port/build/host.log）
Start-Process E:\FD2\port\build\fd2host.exe -ArgumentList '--exit-after=25' -WorkingDirectory 'E:\FD2'

# 加载器自检：把加载结果与 Ghidra 导出的重定位镜像逐字节对比（加载正确性的唯一可信判据）
& E:\FD2\port\build\letest.exe
```

**宿主命令行参数**：`--gamedir <dir>`、`--exe <path>`、`--exit-after <秒>`、`--trace=<n>`
（VEH 单步跟踪）、`--image`（用预导出镜像代替解析 exe）、`--headless`（不画帧）、
`--screenshot=<file.bmp>`、`--shot-frame=<n>`（在第 n 帧把实际送显的 RGB 缓冲导出为 BMP，
默认 300；用于**不依赖桌面/窗口**地核对调色板与通道顺序，见 §8 第 17 条）、
`--autokey=<延时ms:VK[,VK...];...>`（自动按键，用于回归 continue 等菜单路径，见 §11.5）、
`--midi-dump=<file.wav>`（把渲染好的音乐导出为 WAV，离线核对速度/音色，见 §11）。

> **所有参数都支持空格与等号两种写法**（`--gamedir=D:\x` 与 `--gamedir D:\x` 等价，第 13 轮起
> 由 `host_init()` 开头的 `opt_wants_value()` 统一归一化）。早先**每个参数只认一种写法**，
> 另一种会被静默忽略并回退到默认值（`E:\FD2`）—— 第 12 轮第一次对照实验就是这么白跑的，
> 写测试时**先在日志里核对 `host: working directory = …`**（§8-32、§8-33）。

**一键回归**：`pwsh -File E:\FD2\port\regress.ps1`（重建沙箱 → autokey → 断言，见 §12.4）。

### 关键外部工具：Ghidra 本地 HTTP 桥

Ghidra 进程（`javaw`）在 **`http://127.0.0.1:8089`** 上开了一个 HTTP 桥，实测可用端点：

```powershell
Invoke-WebRequest "http://127.0.0.1:8089/read_memory?address=0x10000&length=300"
Invoke-WebRequest "http://127.0.0.1:8089/list_segments"
Invoke-WebRequest "http://127.0.0.1:8089/list_methods"
Invoke-WebRequest "http://127.0.0.1:8089/decompile?address=0x3ccb4"   # 端点名待确认
```

**用途**：`/read_memory` 可批量导出内存并直接写文件，**不消耗模型上下文**——比走 MCP 工具高效得多
（`build/object1..3.bin` 就是这么导出的，作为重定位参考镜像）。
注意：`run_script_inline` 等脚本接口被禁用（需要 `GHIDRA_MCP_ALLOW_SCRIPTS=1`）。

---

## 3. 游戏二进制的已验证事实

### 3.1 容器结构

```
文件偏移 0         MZ 头（DOS/4GW 改造过，e_lfanew 字段不可用）
文件偏移 0 .. 0x27ACC   DOS/4GW 加载器 stub（16 位，含 "RATIONAL DOS/4G"、"WATCOM"、"Phar Lap" 等检测串）
文件偏移 0x27ACC   LE 头（"LE\0\0"）
文件偏移 0x36014   对象数据页起点（页对齐、紧凑）
文件大小 0x7C4E6   509,158 字节
```

### 3.2 LE 头字段偏移（**实测值，与标准 LE 不同**，务必按此解析）

| 相对 LE 头 | 值 | 含义 |
|---|---|---|
| +0x04 | 0 | format level |
| +0x08 | 0x00010002 | cpu=2(80286), os=1(DOS) |
| +0x10 | 0x200 | module flags |
| +0x14 | 71 | module pages |
| +0x18 | 1 | EIP 对象号（1-based） |
| +0x1C | 0x2CCB4 | EIP（相对该对象基址） |
| **+0x40** | 0xC4 | **对象表偏移** |
| **+0x44** | 3 | **对象数** |
| **+0x48** | 0x10C | **对象页表偏移** |
| **+0x68** | 0x22F | **fixup 页表偏移** |
| **+0x6C** | 0x34F | **fixup 记录表偏移** |

对象表项 24 字节：`vsize(4) base(4) flags(4) pageidx(4) npages(4) reserved(4)`

| 对象 | base | vsize | 页号 | 属性 |
|---|---|---|---|---|
| obj0 | `0x00010000` | `0x3EF29` | 1..63 | READ+EXEC+PRELOAD+32BIT |
| obj1 | `0x00050000` | `0x056B0` | 64..67 | READ+WRITE+PRELOAD+32BIT |
| obj2 | `0x00060000` | `0x034D2` | 68..71 | READ+WRITE+PRELOAD+32BIT |

**入口线性地址 = obj0.base + 0x2CCB4 = `0x3CCB4`**（DOS/4GW 的 C 运行库启动代码）。

### 3.3 fixup 记录格式（已 100% 验证）

fixup 页表：72 个 u32（相对记录表起点的累计偏移），页 i（1-based）的记录块 = `[pt[i-1], pt[i])`。
记录表总长 57,497 字节。

每条记录：

```
type 0x07 : [07][size][src:2 LE][obj:1][tgt:2 + (size>>4) 字节 LE]
            size 高 4 位 = 额外目标字节数，目标字节数 = 2 + (size>>4)
            obj  = 目标对象号（1-based）
            动作 : *(u32*)(page_base + src) = objects[obj-1].base + tgt
type 0x00 : 1 字节填充，跳过
```

**必须跳过"源操作数跨页"的记录**（`src + 4 > 0x1000`，共 22 条）。它们会把下一页开头的代码
覆盖掉——不跳过就会崩在 `0x3E000`（已实测）。Ghidra 的加载器同样不应用它们。

验证结果（`letest.exe`）：obj1、obj2 与 Ghidra 重定位镜像**逐字节一致**；obj0 仅剩 11 字节差异
（全部位于页边界，语义待确认，当前认定无害）。

### 3.4 对象数据在文件中的位置（推导法）

对象页数据在文件里**页对齐、按对象顺序紧凑存放**，最后一个对象只占其 `vsize` 余数：

```
总跨度 = Σ(对象 page_count × 0x1000)，但最后一个对象取 (page_count-1)×0x1000 + vsize%0x1000
data_start = 文件大小 - 总跨度            =>  0x36014（本文件）
之后逐对象顺序拷贝，遇到文件末尾截止
```

### 3.5 游戏的平台依赖（实测）

| 子系统 | 实现 | 备注 |
|---|---|---|
| 图形 | 直接读写 `0xA0000`，320×200、8bpp 索引色 | 60+ 处硬编码 `0xA0000` |
| 显示模式 | `INT 10h` AH=0（已实测设 0x13）、AH=0F、AX=1C00 | 由 DOS/4GW stub 调用 |
| 调色板 | `out 0x3C8`（索引）/ `out 0x3C9`（R,G,B） | 宿主已捕获到 `port ops` |
| 声音 | 静态链接 Miles AIL + **外置 16 位实模式驱动** `*.DIG` / `*.MDI` | AIL 会跳进驱动代码执行 ⇒ 必须整体替换 |
| 音乐数据 | 标准 XMIDI（IFF `FORM/XMID`），在 `FDMUS.DAT` | 可用现代合成器播 |
| 键盘 | **直接轮询 BDA**：`0x41A/0x41C`（缓冲 head/tail）、`0x46C`（BIOS tick）、`0x417` | 宿主已重定向到低内存镜像 |
| 鼠标 | `INT 33h` | 待接真实状态 |
| 计时 | 轮询 `INT 21h AH=2C` 的 **DH（秒）** 等变化 | 忙等循环，正常 |
| 文件 | `INT 21h` AH=3D/3E/3F/40/42/44(IOCTL) | 按裸文件名打开，故工作目录必须是游戏目录 |
| 内存 | `INT 21h AH=48`、`INT 31h AX=0x0100 / 0x0501` | 见 §4.4 的返回值语义 |
| DPMI | `INT 31h`（描述符、版本、内存、实模式向量） | |
| 数据文件 | `DIG.INI` `MDI.INI` `*.DIG` `*.MDI` `FDOTHER.DAT` `FDTXT.DAT` `FDSHAP.DAT` `FDFIELD.DAT` `DATO.DAT` `FDMUS.DAT` `BG.DAT` `FIGANI.DAT` `ANI.DAT` `TAI.DAT` `FD2.SAV` `FD2.TMP` `FDICON.B24` | 格式已在 `FD2_analysis.md` 里分析过（DAT 容器 = "LLLLLL" + header_size + 偏移表） |

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

---

## 5. 执行进度（`build/host.log` 实证）

```
LE 加载 + 7937 条 fixup 应用                    ✅
VEH 安装，112 个 int 站点改写为 int3 并接管      ✅
进入游戏入口 0x3CCB4                            ✅
DOS/4GW 环境探测 / PSP / 环境解析                ✅
C 运行库初始化（sbrk 走 INT21、AH=2C 计时）      ✅
AIL 探测驱动 SBPRO2.MDI / SB16.DIG → 被拦截      ✅
打开 DIG.INI / MDI.INI / FDOTHER.DAT / FDTXT.DAT ✅
INT 10h AH=0 设置视频模式 0x13（320×200×256）    ✅
调色板端口 I/O                                   ✅
继续大块分配内存、反复读 FDOTHER.DAT             ✅
开场资源解压 → 帧缓冲 → 持续动画循环              ✅（修复 AH=42 / AH=48 语义之后，见 §8 第 15、16 条）
连续运行 30 s：960 帧 / port ops 80 万+ / 无崩溃  ✅ ← POC 目标"看到画面"达成
```

---

## 6. 历史卡点（**已修复**，保留作诊断参考）

> **状态：已解决（2026-10-04）。** 两个卡点的根因都不在游戏逻辑，而在宿主 INT 21h 的语义：
> `AH=42`(lseek) 把 CX 当成了 64 位偏移的高半（于是文件指针跳到 ~171 GB），`AH=48` 用
> `Ebx & 0xFFFF` 截断请求（把 `0x10000` 截成 0 并"慷慨"给出 16 字节块）。前者让资源读到 0 字节、
> 游戏解压垃圾数据后写飞；后者让 `__ExpandDGROUP` 拿到 16 字节块后越界写元数据。
> 修复内容见 §8 第 15、16 条，诊断手段见第 17 条。修复后连续运行 30 秒无崩溃。

**修掉端口指令长度 bug 之后**（见 §8 第 13 条），端口操作从 4 次涨到 **1027 次**，崩点随之前移，
当时的现象是**解压/填充循环写到了未映射地址**：

```
cpu: ACCESS VIOLATION at 0x4E9F7 (Eip=0x4E9F7) write to address 0xC0005
     code @0x4E9E0: … AC F3 AA 66 0B DB 75 E5 …
     eax=FFFFFFC7 ebx=FFFFE252 ecx=00000001 edx=FFFFF728 esi=03E8031E edi=000C0005
```

- `EIP = 0x4E9F7` 在 obj0 内（`rep stosb`），`EDI = 0x000C0005`；
- **`0xC0005` = `(段值 0xC000 << 4) + 5`** —— 游戏在按"实模式段 → 线性地址"的规则算目标地址；
- 该块内存**当前没有分配**（分配账本里此刻只有 9 个 `INT21 AH=48` 的高地址块，
  `INT31 AX=0100` 的低内存分配这次没发生）。

⇒ 结论：游戏对某些缓冲区的地址假设是"**实模式可寻址（< 1 MiB）**"，
需要让 `INT 21h AH=48h`（以及其它分配路径）在**被要求"DOS 内存"时返回低地址**，
或者把 `0x80000..0xFFFFF` 整段预留/提交，使任何 `段值 << 4` 都落在已映射范围内。

**⚠ 曾经的误判（务必知道，否则会走弯路）**：更早的日志显示
`EIP = 宿主映像基址 + 0x3600`（两次改链接基址都跟着变），符号表定位到 `_fd2_veh@4` 内部，
于是判断为"VEH 处理异常时把自己改坏了"。**这是假象**：

- 真实情况是游戏跳到了 `EIP=0`（`instruction fetch at 0x0`）；
- `fd2_veh` 里 `p = (const uint8_t *)c->Eip` 因此成为 `NULL`，
  规则 (2) 的扫描循环去读 `p[-8]` = `0xFFFFFFF8`，**在处理器内部再次违规**；
- Windows 于是报告第二次异常的 EIP —— 那自然落在 `fd2_veh` 的代码里。

修法已落地：所有"检查故障"的访存都走 `guest_readable()`（基于 `VirtualQuery` 检查
COMMIT/保护位/区域末尾），并且当 `EIP` 本身不可读时直接打印"游戏跳进了未映射内存"后退出，
不再继续扫描。异常类型也按 Windows 约定区分：`ExceptionInformation[0]` = 0/1/8 →
read/write/**instruction fetch**（之前把 8 误报成 "write"）。

**当时的下一步（均已执行，留档）**
1. ✅ **修 `INT 21h AH=42`(lseek)**：CX:DX 是入参 32 位偏移、DX:AX 是出参新偏移。原实现把 CX
   当作 `SetFilePointer` 的**高 32 位**（跳到 ~171 GB，越 EOF 却不报错），返回值也只对"读 EAX
   全 32 位"的调用方成立。修好后资源数据立刻正确 —— 崩溃前 `RLE w=320`，此前读到的是垃圾宽高。
2. ✅ **预映射 1 MiB 内的实模式区**：`dos_init_lowmem` 现在把 `0x80000..0x9FFFF` 与
   `0xC0000..0xFFFFF` reserve+commit；`INT31 0100` 改为纯账本分配（页面已存在，再调
   `VirtualAlloc` 反而会失败）。真机上 0xC0000+ 是 ROM、写入被丢弃，所以游戏**合法地**会越过
   VGA 窗口写到那里，宿主必须让这些地址可写。
3. ✅ **修 `INT 21h AH=48`**：读**完整 EBX**（Watcom `_ExpandDGROUP` 传 `0x10000`），
   并让 EBX=0 按 DOS 规范返回失败（旧的"0 paras → 16 字节"兜底正是第二次崩溃的成因）。
4. ✅ **崩溃转储增强**：现在会打印 RLE 状态 `w/h @0x627B4`、`[ESI]` 源字节、`[ESP]` 返回地址、
   EBP 帧的前 6 个参数 —— 本次两次崩溃都是靠这些字段直接定位的（§8 第 17 条）。

---

## 7. 下一步计划（按优先级）

1. **显示层现代化**（当前第一优先）：把 `0xA0000` 的 8bpp 索引缓冲 + `dos_palette` 上传为
   D3D11/OpenGL 纹理 + palette shader。现在的 GDI `StretchDIBits` 每帧做 64000 次查表 +
   软件缩放，是 POC 版；`--screenshot` 导出的 BMP 可作为现代化后的对拍基准。
2. ~~**AIL 替换层**~~ **已完成**（见 §11）：16 个入口已替换为宿主实现，音效走 waveOut、音乐走
   自带合成器 + `gm.dls` 采样。2026-10-05 又修正了 XMIDI 解析（delta 累加 / Note-On 自带音长）
   与 tick 基准（60 ticks/beat），并让采样保留自身包络。
3. ~~**输入 / 鼠标 `INT 33h`**~~ **已判定为不需要**（第 12 轮，见 §12.3）：静态扫描显示游戏从不发
   `INT 33h`，`push 0x33` 都是标志位索引，运行期 `int 33` 调用数为 0；键盘侧已够用
   （片头走 BDA 轮询、菜单走 `INT 16h AH=10h`），`--autokey` 可做无人值守回归（§11.5）。
4. **逐步源码化（路线 C 主体）**：从已理清的模块开始把机器码换成 C 源码 ——
   资源加载（`sub_111BA`）、RLE 解压/blit（`sub_4E98D`）、脚本 VM（`sub_15F84`）、
   游戏工具库（0x4DED4..0x4EF29），最终产出可编译的 x86-64 引擎。
   逆向工作台与测绘起点见 `port/re/RE_MAP.md` + `port/re/funcmap.csv`（§10）。
5. **继续玩**：现在能进剧情画面了，下一批要验证的是战斗/地图等更深路径
   （`--autokey` 走不同的按键序列 + `--screenshot` 逐帧核对）。
6. **稳定性长跑**：连续运行 5 分钟以上与反复重启验证（游戏退出路径已验：`INT10 mode 3` →
   `AH=4Ch` → `ail: shutdown`，见 §12.4）。
7. ~~**文件写入 / 存档路径**~~ **平台侧已补完**（§12）：`AH=3C/41` + `AH=40 CX=0` 截断，
   `regress.ps1` 回归 8/8 PASS。**剩余**："新游戏 → 首次存档 → `FD2.SAV` 从无到有"与
   "存档变小后的截断对拍"两条还没实测；`AH=49/4A` 仍是空操作（账本只增不减）。

---

## 8. 踩坑清单（**务必先读，能省几天**）

1. **别用大静态数组**：`static uint8_t x[8MB]` 会把宿主映像撑到 8.6 MB，ASLR 只能把它放到
   `0x10000` 附近，正好压住游戏对象窗口 ⇒ 每次启动都失败。改成 `VirtualAlloc` 后映像 288 KB，
   问题消失。
2. **别关 ASLR**：`/DYNAMICBASE:NO` 时 Windows 会**自己**占用 `0x10000..0x6FFFF`（实测
   `CMT MAPPED`），游戏对象无法映射。
3. **CRT 堆从 `0x10000` 往上长**：必须在 CRT 初始化之前抢预留，所以用自定义入口
   `fd2_entry`（`/ENTRY:fd2_entry`）先调 `le_reserve_address_space_early()`，
   再调 `mainCRTStartup()`。在 `main()` 里做已经太晚。
4. **低 64 KiB 不可映射**：`VirtualAlloc(0x400, …)` 必然失败，PSP/BDA 访问只能靠重定向或模拟。
5. **DOS/4GW 私有选择器**：启动代码会把 `0x24` 之类当段选择器加载，而本进程 GDT 槽 4 是**代码段**
   ⇒ `#GP`。必须替换成宿主平坦数据选择器。
6. **`INT 21h AH=30h` 的扩展器签名分支**：返回 `'XD'`(0x4458) 会走一条假设"扩展器已就绪"的路径
   （环境指针为 0 ⇒ 崩）；`'BC'`(0x4243) 才是会去读 `PSP:0x2C` 的路径。当前用 `'BC'`。
7. **`INT 21h AH=0xFF` 必须非 0**，否则同样走进空环境指针。
8. **`INT 21h AH=48h` 返回线性地址**（调用方直接解引用）；**`INT 31h 0501h`** 必须真分配
   （`BX:CX` 进/出参），否则游戏拿野指针。
9. **`INT 21h AH=2Ch` 要清零 AL**（DOS 行为），否则调用方 `cmp al,0` 会走错分支。
10. **工作目录必须是游戏目录**：游戏按裸文件名打开资源，否则全部 `ERROR_FILE_NOT_FOUND`。
11. **fixup 跨页记录必须跳过**，否则改写下一页开头代码（崩在 `0x3E000`）。
12. **AIL 的 `*.DIG`/`*.MDI` 是 16 位实模式代码**，AIL 会跳进去执行 ⇒ 在任何路线下都必须整体替换；
    POC 阶段直接把这两类文件报"不存在"，让 AIL 以"无设备"启动。
13. **特权指令模拟必须返回"指令长度"，不是"操作数大小"** —— 这条踩得最狠，浪费了一整轮定位：
    ```c
    EE  out dx, al     长度 1     （曾误返回 2）
    EF  out dx, ax     长度 1     （曾误返回 2）
    ED  in  ax, dx     长度 1     （曾误返回 2）
    E4  in  al, imm8   长度 2     （曾误返回 1）
    E5/E6/E7           长度 2
    ```
    长度错 1 字节 ⇒ EIP 错位 ⇒ 执行流跑飞（表现为"跳到地址 0"或写随机地址）。
    修好后 `port ops` 从 **4 次涨到 1027 次**，是判断这类 bug 是否修好的直接指标。
14. **别让诊断代码自己崩**：`fd2_veh` 在检查故障时会读 `EIP` 附近的字节、`[EAX]`、栈顶；
    一旦 `EIP` 本身无效（例如 0），这些读取会**在处理器内部再次触发异常**，
    报告出来的 EIP 就变成了处理器自己的地址，看上去像"处理器把自己改坏了"。
    所有这类访存必须走 `guest_readable()` 先做有效性检查。
15. **`INT 21h AH=42`(lseek) 的 32 位约定有两处**：入参是 **CX:DX**（CX 高 16、DX 低 16，
    不是"EDX 全 32 位"），出参是 **DX:AX**。原实现把 CX 当成 `SetFilePointer` 的
    `plDistanceToMoveHigh`（64 位偏移的**高 32 位**），于是 `CX:DX = 0x002A:1CF3` 被定位到
    `(0x2A<<32)|0x1CF3` ≈ 171 GB —— **合法的 64 位位置、不报错、直接越过 EOF**，随后的
    `ReadFile` 返回 0 字节，游戏便拿旧垃圾数据去解压。正确写法：
    `pos = ((Ecx & 0xFFFF) << 16) | (Edx & 0xFFFF)` → `SetFilePointer(h, pos, NULL, meth)`，
    返回时让 `EAX` = 完整 32 位新位置、`DX` = 高 16 位（两种调用方读法都对）。
16. **`INT 21h AH=48` 要读完整 EBX，且 EBX=0 必须失败**：Watcom 的 `_ExpandDGROUP`(0x3D842)
    用 `mov ebx, esi` 传**字节数** `0x10000`（64 KiB 段），只取 `BX` 会截断成 0；而
    "0 paras 就当 16 字节分配"的兜底更糟 —— 分配器按整段使用，越界写到 `0x48FFFF8`。
    DOS 自己对 `BX=0` 就是返回失败（CF=1/AX=8），宿主照做才不会把游戏推上
    "拿到块但其实没有"的路径。
17. **调色板有两层坑：6 位 DAC + DIB 是 BGRA**：
    - VGA DAC 每通道只有 **6 位（0..63）**；把原始值当 8 位显示 ⇒ 亮度/饱和度只剩约 25%。
      `dos_palette` 统一保存 8 位值：捕获时先 `& 0x3F`，再 `(v << 2) | (v >> 4)` 伸展。
    - 32bpp `BI_RGB` 内存序是 **BGRA**（byte0=B、byte1=G、byte2=R）。写成
      `(c[2]<<16)|(c[1]<<8)|c[0]` 会把**红蓝互换** ⇒ 黄色（R+G）显示成青蓝色。正确：
      `(c[0]<<16)|(c[1]<<8)|c[2]`。
    - `--screenshot=<file.bmp>` 导出实际送显缓冲，可完全绕开窗口/桌面来核对这些。
18. **替换 AIL 只需改写入口**：游戏只调用 16 个 `AIL_*`（清单见 `re/RE_MAP.md` §3），从不直接调用
    AIL 内部函数 ⇒ 把入口前 5 字节改成 `jmp rel32` 就够，不必模拟 AIL 内部状态机。AIL 是 Watcom
    cdecl，与 MSVC `__cdecl` 在这些签名上 ABI 兼容。注意 **`AIL_install_DIG_INI` 必须返回非 0**：
    游戏据此设置"有数字音设备"标志 `byte_53EF1`，为 0 时 `sub_25A96`/`sub_25B45` 会直接跳过播放。
19. **XMIDI 不是标准 SMF**：delta 为 0 时**省略**、事件可以只写数据字节（running status）、
    时间基准是 **60 ticks/beat 而非固定 Hz**（要按 `FF 51 03` 的 tempo 换算，否则曲子长度差一倍
    以上）、并且本作几乎不含 note-off（需按通道单音处理）。详见 §11。
20. **包络在 note-on 的同一采样返回 0 会"吞噬"整个音符**：软件合成器第一版渲染出
    `non-silent 0.0%、took 0 ms` —— attack 包络从 0 开始，混音循环看到 `e <= 0` 就把 voice 标成
    inactive，而"无活跃 voice"又触发"快进到下一个事件"，于是**没有任何采样被真正合成**。
    修法：包络在 `age == 0` 时返回 `(age+1)/attack`，且**只有进入 release 之后**才允许关闭 voice。
21. **系统 MIDI（GS Wavetable Synth）不可靠**：`midiOutOpen` 会成功、设备列表里也在，但
    `midiOutGetVolume` 返回 `MMSYSERR_NOTSUPPORTED`，其电平由系统混音器控制 —— 被静音时
    应用侧既听不到也管不了。音乐因此改为自带合成器走 waveOut（§11.1）。诊断顺序建议：
    先 `--midi-test` 听测试音，再看是否该换后端。
22. **键盘输入有两条完全不同的路径**（这坑很隐蔽）：
    - **片头"任意键跳过"= 轮询 BDA**：`sub_10620` 只比较 `0x41A/0x41C`（head/tail），然后
      `sub_4E381` 清缓冲 —— 所以"按键有反应"**并不说明按键数据被正确解析**；
    - **菜单导航 = `INT 16h AH=0x10`（读扩展键）**：`sub_26152` 按返回的 **AH（扫描码）** 判断
      —— `0x4B` 左 / `0x4D` 右（各带一个导航音效）/ `0x22` 换音乐 / `0xE0`、`0x52` 视为确认(0x1C)。
    宿主原先**完全没有实现 vector 0x16**，于是 `int386(0x16)` 返回时 AX 原封不动，游戏把残留的
    功能号 `0x10` 当扫描码 ⇒ **片头能跳过但菜单方向键全无反应**。
    现在实现了 `INT 16h` 的 `AH=0/1/2/0x10/0x11`（`kbd_fetch()` 从 BDA 取键并推进 head），
    并且按键写入缓冲区时**扩展键的 ascii 字节用 `0xE0`**、普通键用 `ToAscii()` 生成真字符。
23. **只 patch"游戏直接调用的"AIL 入口不够 —— 必须把全部导出封掉**。症状很迷惑：能进菜单、能上下
    选择，但选 **continue** 后游戏**直接退出**，日志只有 `unhandled exception 80000003 at 0x3B89A`。
    根因链：游戏（或库）**经非直接调用路径**到达 `AIL_install_timbre`（**0x3B80F**，靠它自己的
    `"AIL_install_timbre(...)"` trace 串才定位到 —— 它没有直接 call 者，IDA 甚至不把它识别为函数）
    → 执行**原始** AIL 代码 → 内部 `call sub_44AF0` → 该函数依赖 `AIL_startup` 建立的**驱动/定时器表**
    （我们的 stub 什么也没建，`install_*` 还返回了假句柄 1）→ 读到野指针 → **执行流跳到指令中间**
    （`0x3B89A` 恰好是 `0x3B898` 处 `jz rel32` 的操作数中间字节）→ 撞上偶然的 `0xCC` → 未处理断点 → 退出。
    **修法**：把 **51 个导出 + `AIL_install_timbre` 共 52 个入口全部 patch**，游戏没直接调用的用通用
    stub（`return 0`）。只要不让任何原始 AIL 代码跑起来，就碰不到它内部的驱动依赖。
    **判据/手法**：这种漏网入口 `XrefsTo()` 为空、`get_func_attr()` 返回 `BADADDR`，只能靠
    **trace 串的引用点** 或下面的异常环形记录发现。
24. **"执行流跳飞"要用环形记录定位**：只在崩溃点打印是不够的。现在 VEH 会记录最近 32 个
    **非 int3** 异常（特权指令 / 访问违规 / 野断点）——因为正常 int3 站点每分钟上百万次，把有效的
    异常序列淹没。配合"未处理异常也打印完整现场（寄存器、EIP 前后 16 字节、栈顶 8 字、最后中断站点）"，
    才能看出是"跳进了指令中间"而不是"游戏逻辑本身出错"。
    识别技巧：**EIP 落在某条指令的字节中间**（例如 `0x3B89A` 是 `jz` 的第 3 个字节），且 `eax` 里出现
    某条指令操作码的**字节反序**（本例 `eax=840F0008`，高 16 位 `0x840F` 就是 `0F 84` 反序）。

    > **2026-10-05 更正**：第 23 条把"continue 退出"归因于"AIL 入口漏网"，**归因错了**。
    > 真凶见第 25 条：`call sub_34894` 的**位移字节** `CD 20` 被主机的 `int` 站点扫描改写了，
    > 执行流是**被动**跳进 `AIL_install_timbre` 中部的 —— 与"AIL 内部依赖驱动表"无关。
    > 把 52 个入口打桩仍然值得保留（防间接调用），但它不是这次崩溃的原因。

25. **用"字节模式扫描"改写游戏代码是危险的：`CD 20` 破坏了 `call` 的位移** —— 这就是"点 continue
    就退出"的真凶，卡了两轮。
    - 旧做法：全局扫 `CD xx`（白名单含 `0x20`）并改写成 `CC 90` 建立 int3 站点。
    - `call sub_34894` 的机器码是 **`E8 CD 20 02 00`**（位移 `0x000220CD`）—— 位移里含 `CD 20`，
      被改成 `E8 CC 90 02 00` ⇒ 调用目标变成 **`0x3B893`**，落在 `AIL_install_timbre` 函数体中部，
      顺着 AIL 的调试打印路径跑飞，最后撞上 `0x3B89A` 那个本来就等于 `0xCC` 的字节 ⇒
      `unhandled exception 80000003` ⇒ 退出。
    - **判据**：拿 obj0 里全部 `CD xx` 候选与反汇编器的指令边界对照 —— 112 个候选里只有 **1 个**
      不是真正的 `int` 指令（就是它）；另外 12 个"假阳性"其实是 DOS/4GW 的 `CD NN C3` 中断桩
      表（@0x46948），是真代码。
    - **修法**：不再改写游戏代码。`src/probe4.c` 实测（实证优先）：ring3 执行 `int NN` 抛
      `EXCEPTION_ACCESS_VIOLATION`、`int 3` 抛 `EXCEPTION_BREAKPOINT`，**EIP 都指向该指令本身**，
      于是在 VEH 最前面读 `[EIP]==0xCD` 取向量、跳过前缀 +2 字节直接分派即可
      （此时 `int 0x21` 约 400 万次/25 秒，帧率不变）。
    - 同类教训：**任何"扫字节改代码"都必须先证明该字节确实位于指令边界**。

26. **XMIDI 的 delta 是"连续 `<0x80` 字节的累加和"**，不是单字节、也不是 SMF 的移位拼接 VLQ，且
    **没有任何 running status**（每个事件都写 status）。详见 §11 —— 猜错这一条会让整首曲子的
    时间轴全错。

27. **XMIDI 的 Note-On 自带音长 VLQ，文件里几乎没有 Note-Off** —— 必须把每个 note-on 展开成
    `tick + 音长` 处的显式 note-off。不读这个字段 ⇒ 所有音符靠超时释放 ⇒
    **"没有节奏、音色被拉长"**。详见 §11。

28. **XMI 的 tick 基准是 60 ticks/beat**（`tick_rate = 60e6 / tempo_us`）。改成 120 会**快一倍**；
    判据是两个独立实现（WildMIDI `xmi2mid.c`、`fd2_re` 的 xmi2mid.py）都得出 60。详见 §11。

29. **不要把合成包络套在 GM 采样上**：DLS 采样自带包络（钢琴衰减、弦乐持续、鼓是一次性），
    再套一层 A/D/S(0.70) 会把所有乐器压成同一种"扁"音色 —— 这是"音色缺"的另一半原因。
    采样 voice 现在只做 1 ms 起音 + note-off 处的淡出，波形回退路径才用原来的合成包络。

30. **`INT 21h AH=3C`(CREAT) 缺失 = "文件不存在就崩"**（第 12 轮，卡点已修复但留档）：
    Watcom 的 `sopen()` 先 `AH=3D` 打开，**失败且带 `O_CREAT` 时退回 `AH=3C`**，close 后再 open ——
    `fopen("wb")` 对尚不存在的文件（fresh install 的 `FD2.TMP`、首次保存的 `FD2.SAV`）必走这条。
    宿主没实现 ⇒ CF=1 ⇒ CRT 返回 **NULL `FILE\*`** ⇒ 游戏 `fwrite(NULL,…)` 解引用 `FILE+0xC` ⇒
    **`AV at 0x377B2 read from 0xC`**（指令 `F6 43 0C 02` = `test byte [ebx+0xC],2`，`EBX=0`）。
    判据：日志里紧挨着的 `dos: open '<名>' -> FFFFFFFF (2)` + `UNHANDLED INT21 AH=3C`。

31. **`AH=40` 写 0 字节在 DOS 里是"截断"，在 Windows 里是空操作**：Watcom `sopen()` 的
    `O_TRUNC`（也就是 `fopen("wb")`）就是靠"打开后写 0 字节"把旧内容清掉的。宿主直接调
    `WriteFile(…,0,…)` 等于什么都没做 ⇒ **存档比上一次短时，旧存档的尾巴会残留**（数据损坏，
    而且不报错）。必须 `SetFilePointer(FILE_CURRENT)` + `SetEndOfFile()`（§12.2）。

32. **命令行参数写法不匹配会"静默用默认值"**：`--gamedir=<dir>` 只认 `--gamedir <dir>` 时
    不会报错，而是悄悄回退到 `E:\FD2` —— 对照实验因此跑错了目录、结论差点反过来。
    宿主现在两种写法都认；写测试脚本时**先在日志里核对 `host: working directory = …`**。

33. **做参数归一化时别把 `argv[0]` 丢了**（第 13 轮，实现双写法时踩到）：把 `--opt value`
    合并成 `--opt=value` 时，如果新数组 `av[0]` 放的是**第一个选项**，而解析循环仍是
    `for (i = 1; i < argc; …)`，就会**静默跳过第一个选项**。症状极具迷惑性：
    `--exit-after 6` 生效（它恰好落在 index 1）、`--gamedir x` 却回退默认目录 —— 看起来像
    "只修好了一半"。判据依旧是日志 `host: working directory = …`。
    正确写法：`av[0] = argv[0]; ac = 1;` 再从 `i = 1` 合并（`host_init()` 开头）。

---

## 9. 调试手册

| 需求 | 做法 |
|---|---|
| 看宿主运行日志 | `port/build/host.log`（WINDOWS 子系统，不弹控制台） |
| 验证加载是否正确 | `letest.exe`（与 `build/object1..3.bin` 逐字节对比） |
| 重新导出 Ghidra 重定位镜像 | HTTP 桥 `/read_memory` 循环写文件（见 §2） |
| 反查崩溃地址属于哪个函数 | `port/fd2host.map`（RVA = 地址 - 映像基址；section:offset = RVA - 0x1000） |
| 跟丢执行流 | `--trace=<n>` 单步跟踪（VEH 里置 TF，注意每次单步异常后要重新置位） |
| 看谁改了内存 | 崩溃报告会打印 EIP 前后 48 字节 + 分配账本（`note_alloc`） |
| 反汇编游戏函数 | Ghidra：`decompile_function` / `read_memory`；注意 obj0 是 32 位平坦代码，obj1/obj2 是数据 |
| 已知的 Ghidra 陷阱 | `_entry`(0x3CCB4) 的反编译里充满 `in_DS/in_ES/swi()` 伪寄存器——那是段寄存器访问与 `int` 指令的建模，代码本身是正常的 32 位代码 |
| 抓当前帧画面（不依赖窗口/桌面） | `--exit-after=30 --screenshot=E:\FD2\port\build\frame.bmp --shot-frame=700`，日志出现 `host: frame 700 dumped` 后把 BMP 转 PNG 查看（`[System.Drawing.Image]::FromFile`）。帧数见 watchdog 行 `(N frames drawn)` |
| 崩溃现场新增字段 | AV 转储现在含 `RLE w/h (@0x627B4)`、`[ESI]` 源字节、`[ESP]` 返回地址、EBP 帧的 6 个参数 —— 定位"解压写飞"与"分配器越界"两类问题最快 |
| 文件写入回归（一键） | `pwsh -File port\regress.ps1`：重建 `build\sandbox`（删掉 `FD2.TMP`）→ `--autokey` 走 continue → 对日志+文件系统断言 8 项，`ALL PASS` 为准（§12.4） |
| 手工复现 fresh install | 把数据文件拷到任意目录、**删掉 `FD2.TMP`**，再 `--gamedir <该目录> --autokey=5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN` |
| 查平台层还缺哪些服务 | ida MCP 裸扫 obj0 的 `CD xx` + 回看 `int 21h` 前的 `mov ah,imm`：产物 `re/int_sites_all.txt`、`re/int21_ah_used.txt`；宿主侧对照 `src/dos.c` 的 `switch (ah)` |

---

## 10. IDA Pro 逆向环境（ida MCP，2026-10-04 建立）

**源码转译的主分析环境**。完整测绘见 **`port/re/RE_MAP.md`**（函数分区、AIL 边界、核心函数档案、
转译路线），全量函数表见 `port/re/funcmap.csv`（1359 行）。

| 项 | 值 |
|---|---|
| 工具 | ida MCP（`use_capability` → `mcp-tool:ida/open_database \| execute_python \| reference \| save_database`） |
| IDA | IDA Professional 9.5 + Hex-Rays (x86) |
| 数据库 | `E:\FD2\FD2.EXE.i64`（4.4 MB，已含 51 个 AIL 函数重命名）；直接打开 .i64 可跳过重新分析 |
| 已验证 | IDA 段布局/入口与 Ghidra **逐项一致**（cseg01=0x10000、入口 0x3CCB4），fixup 已被 IDA 应用 |

**一句话结论**：游戏区 569 个函数（0x10000..0x37000）**零条 `int` 指令**，DOS 交互全走 CRT 包装
（`int386`/`fopen`/`sbrk`…）⇒ 分层转译成立：游戏逻辑 = 纯计算 + `0xA0000` 显存访问；平台层只需给
CRT 提供 Win32 实现；Miles AIL 的 51 个 `AIL_*` 入口整体打桩替换。

**IDA 9.5 API 陷阱**：用 `ida_hexrays.decompile(ea)`（**没有** `idc.decompile`）；`idautils.Strings()`
返回 list（无 `.setup()`）；`idautils.Entries()` 返回 4 元组；批量分析结果**写文件**而不是返回上下文。

---

## 11. 声音：AIL 替换层（2026-10-04 实现）

**背景**：Miles AIL 已静态链接在 `FD2.EXE` 里，但它靠 **16 位实模式驱动**（`SB16.DIG` /
`SBPRO2.MDI`）发声 —— AIL 会跳进驱动代码执行，这在 Win32 进程里不可能。此前宿主把这两个文件
报"不存在"，于是 `AIL_install_*` 全返回 0，游戏设了"无数字音设备"标志后**静音运行**。

**做法**：IDA 查明游戏**只调用 16 个 AIL 入口**（清单见 `re/RE_MAP.md` §3），宿主把这 16 个入口的
前 5 字节改写为 `jmp rel32` 跳到 `src/ail.c` 的实现 —— AIL 是 Watcom cdecl，与 MSVC `__cdecl`
对这些签名 ABI 等价（参数在栈、调用方清栈、返回值在 EAX），因此**不需要 thunk**。

| 通路 | 实现 |
|---|---|
| **数字音效** | `src/ail.c` + WinMM **waveOut**：游戏通过 `AIL_set_sample_address(h, ptr, len)` 交出的 PCM 被拷贝后提交给声卡；配套实现 `AIL_init_sample` / `_set_sample_loop_count` / `_start_sample` / `_stop_sample` / `_allocate_sample_handle` |
| **音乐** | `src/xmidi.c`：`AIL_init_sequence(h, addr, num)` 给的是 **FDMUS.DAT 里 XDIR 目录的地址**，按 AIL 自己的 walker（`sub_42520`）取第 `num` 首 `FORM XMID`，解析 `EVNT` 后用 **Windows MIDI Mapper**（GS Wavetable Synth）回放 |

**实测（host.log）**：

```
ail: patched 16 AIL entry points to host implementations (11025 Hz, 8-bit, 1 ch)
ail: install_DIG_INI -> fake driver handle 1                 ← 让游戏走进音频分支
ail: set_sample_address len=41333  range=60..A0  near-0x80=100%   ← 8 位无符号 PCM 的铁证
ail: play 41333 bytes (3.75 s @ 11025 Hz, loop=1)
xmidi: 2838 events, 35445 ticks, tempo 535714 us/beat (112.0 BPM) -> 316.5 s at 112.0 ticks/s,
       50 skipped bytes, loop=0
```

**采样格式的判定依据**（不靠猜）：游戏**从不调用** `AIL_set_sample_type` / `_playback_rate`，
所以走 AIL 默认值（`DIG_F_MONO_8`、11025 Hz）。运行时统计显示样本字节 **100% 落在 0x60..0xA0
且围绕 0x80**（8 位无符号 PCM 的中心值），并且存在**奇数长度**的样本 ⇒ 排除 16 位。
可用 `--ail-rate` / `--ail-bits` / `--ail-stereo` 覆盖这些假设。

**XMIDI 事件流 ≠ 标准 SMF**（2026-10-05 彻底修正，旧结论是错的）：

XMIDI 有三条规则与标准 MIDI 完全不同，最初实现按 SMF 直觉写，结果**整首曲子的时间轴全错**：

1. **delta = 连续 `<0x80` 字节的【累加和】**，直到遇到下一个 status 字节（≥0x80）为止。
   - 不是 SMF 的"移位拼接"VLQ，也不是单字节；
   - **不是**"delta 为 0 时省略"那么简单 —— `20 20 91` 是 delta = 64 而不是 delta = 32 + 数据；
   - 权威实现：WildMIDI `xmi2mid.c` 的 `GetVLQ2()`（累加、遇 status 回退）；`fd2_re` 同游戏的
     逆向文档 `07-music-xmidi-format.md` 也写明"間隔累加"。
2. **没有 running status**：每个事件都写 status 字节，所以 delta 之后**必定**是 status。
   旧实现为了"区分 delta 与数据字节"去猜 `status_ok()`，反而把大量 delta 吞成了数据字节。
3. **Note-On 自带音长**：`9n note velocity <VLQ 音长>`，**XMIDI 不发 Note-Off**，播放器要在
   `当前 tick + 音长` 处排程关音。这里音长用的是**标准 VLQ**（与 delta 不同！）。
   旧实现没读这个字段 ⇒ 每个音符都靠 2.5 秒超时释放 ⇒ **听感就是"没有节奏、音色被拉长"**。

**tick 基准 = 60 ticks/beat**（XMI 无 division 字段），墙钟时间由 `FF 51 03` tempo 决定：
`tick_rate = 60 × 1e6 / tempo_us`。本例 535714 µs/beat = 112 BPM ⇒ 112 ticks/s。

- 判据（两个独立来源一致）：WildMIDI 的换算在默认 500000 µs/beat 下给出 **8.3333 ms/tick**，
  正好是 60 ticks/beat；`fd2_re` 用 PPQN=60 转换同一批文件，报告 2256 音符那首为 **137 拍**，
  而我们解析它得 **8237 ticks ÷ 60 = 137.3 拍** ✓（曾误改成 120，会快一倍）。
- 解析自洽性判据（比"听起来像"可靠）：解析后**恰好消费完 EVNT**（0 个无法识别字节），
  且 note-on 与生成的 note-off **一一对应**（本作 2256 / 2255）。

**诊断参数**：
`--ail-dump=<dir>` 导出样本与 XMIDI 原始数据（`ail_smp_*.bin` / `ail_seq_*.bin`）；
`--midi-dump=<file.wav>` 把渲染好的音乐写成 16-bit 单声道 WAV —— **绕开声卡离线核对速度与音色**，
这是本次定位"慢/拉长"最有效的工具。`--autokey=<延时:VK,...>` 自动按键（见 §11.5）。

### 11.1 为什么音乐最终不用系统 MIDI，而是自带合成器

第一版音乐用 Windows MIDI Mapper 回放（`midiOutShortMsg`），但**实际听不到**。诊断结论：

- 系统只有一个 MIDI 设备 `Microsoft GS Wavetable Synth`（存在、32 复音、`midiOutOpen` 成功）；
- **`midiOutGetVolume` 返回 `MMSYSERR_NOTSUPPORTED (8)`** —— 该设备不通过 API 暴露音量，它的电平是
  **系统混音器的 "SW Synth"/MIDI 通道**；被静音时游戏既无法感知也无法修正；
- 事件本身没有问题：2781 个 note-on、覆盖 11 个通道、平均音符间隔 0.35–6 秒、`CC7=127`。

⇒ 系统 MIDI 通路"不可控且可能静默"，所以音乐改为 **`src/synth.c` 自带软件合成器**，渲染成 PCM 后
走**与音效同一条、已验证可用的 waveOut 通路**：

- 每个 MIDI 通道一个 voice（游戏用 11 个）；波形 = 基频 + 2 次 + 3 次谐波（1024 项正弦查表）；
- 线性 A(4 ms)/D(90 ms)/S(0.70)/R(150 ms) 包络；通道 9（鼓）用 60 ms 短释放；
- 音符开关时间由事件 tick 精确换算到采样点；**无音符的区段直接 `memset` 跳过**，所以 5 分钟的曲子
  只需 **344 ms** 渲染；
- 结果为 16-bit 单声道 22050 Hz、13.4 MB，用 `malloc` 分配（**不能用静态数组**，见 §8-1 的 ASLR 陷阱）；
- 循环播放用 200 ms 轮询 `WHDR_DONE` 重新提交同一 buffer；
- 客观验证：`non-silent 100.0%, peak 7993/32767` —— PCM 确实有信号。

`--midi-backend=winmidi` 可切回系统 MIDI 通路；`--midi-test` 会先播一个测试音，用于判断
"系统合成器在这台机器上是否真的出声"。

### 11.2 多音（2026-10-04 修正）

第一版把**每个 MIDI 通道当作单音**，新音符会掐断同通道上一个音符 —— 和弦与重叠声部因此全部丢失。
现在改为：

- **64 个 voice 的池**，按 `(channel, note)` 分配，同音符重触发复用同一个 voice；
- **采样音符用自身包络**，音符长度来自 XMIDI Note-On 内嵌的音长（§11 规则 3）；只有拿不到
  note-off 的极少数音符才靠年龄超时释放（`NOTE_MAX_MS = 2.5 s`，打击乐 250 ms）；池满时偷最老的；
- 实测 `polyphony 36`（64 池），`non-silent 95.7%, peak 32258/32767`。

### 11.3 原版音色：解析 gm.dls（2026-10-04）

波形合成器虽然出声，但音色明显"电子化"。原版音乐用的是 General MIDI 音色，而 Windows 自带的
`C:\Windows\System32\drivers\gm.dls`（3.4 MB，DLS Level 1）**就是那个音色库** —— 系统合成器
本身不出声（§11.1），但**音色库文件是完好的**，可以直接读。

`src/dls.c` 只解析需要的那部分（先用 Python 在真实文件上把结构走通，再写 C）：

| 块 | 内容 |
|---|---|
| `RIFF DLS` → `colh` | 乐器数（本机 235） |
| `LIST lins` → 每个 `LIST ins ` | `insh`（bank / program）+ `LIST lrgn`（region 列表） |
| `LIST rgn ` | `rgnh`（键 / 力度范围）+ `wsmp`（unity note、微调音分）+ `wlnk`（采样索引） |
| `LIST wvpl` → `LIST wave` | `fmt `（PCM 16 位单声道 22050 Hz）+ `wsmp`（unity note、循环点）+ `data` |

播放侧：`synth.c` 的每个 voice 命中采样时改用**线性插值重采样**
（`step = 音符频率/unity 频率 × 采样率/输出率`，含 region 的微调音分）并遵守**循环点**，
无循环的采样播完即释放；没命中才退回波形合成。`ptbl` 偏移表其实用不上 —— `wvpl` 中 `wave`
块的排列顺序就是 `wlnk` 的索引。

**实测**：`dls: 235 instruments, 495 waves loaded`、
`rendered 2781 notes (2781 with GM samples)` —— **全部音符都用了原版采样**，0 个退回波形；
`peak 32300/32767`、`non-silent 100%`、`took 688 ms`。

`--gm-bank=<path>` 可换成其它 DLS 音色库；文件缺失时自动退回波形合成。

### 11.4 打击乐与"少音轨"的修正（2026-10-04）

反馈是"只有一条背景音轨，鼓声和有节奏的乐器听不到"。定位到两个确定性问题：

1. **GM 打击乐不是 bank 号，而是 `ulBank` 的 bit31 标志**。`gm.dls` 里 9 个鼓组的
   `ulBank = 0x80000000`（program 0/8/16/24…，每组 61 个 region），库里**根本没有 bank=128 的乐器**。
   另外 MIDI 的 bank select 编码是 `(CC0 << 7) | CC32`，而 DLS 存的是 `(msb << 8) | lsb`，两者需要转换。
   原来的写法会让鼓音符 fallback 到"按 program 匹配"的**旋律乐器** —— 200 个鼓音符就是这样被吞掉的。
2. **没有 note-off 的打击乐占着 2.5 秒的 voice 槽**（自动释放用了统一年龄）：密集鼓点会填满 voice 池
   并不断"偷"掉旋律声部。现在打击乐用 250 ms 短释放，voice 池扩到 64，偷取时优先挑**已在释放中**的 voice。

同时补上了 **CC7（通道音量）/CC11（表情）** —— 游戏用它们控制各音轨电平，忽略会让所有音轨都按满音量播放。

实测：`rendered 2781 notes (2781 GM samples, 200 drums), polyphony 31`（64 池，不再打满）。

### 11.5 "continue 就退出"的真凶：int 站点扫描改坏了 `call` 的位移（2026-10-05）

**症状**：片头、菜单、音乐都正常，选 **continue** 后游戏直接退出，日志只有
`cpu: unhandled exception 80000003 at 0x3B89A`（撞上了一个 `0xCC` 字节）。

**定位链**（每步都有硬判据）：

1. 崩溃现场 `ESP` 指向 `sub_34894` 的返回地址 ⇒ 正在执行的应是 `sub_34894`；但 `sub_34894`
   的全部指令里**没有任何跳转**，`EIP` 却跑到了 AIL 区。
2. 反查调用点：`sub_127A9` 里 `0x127C2` 处是 `call sub_34894`，机器码 **`E8 CD 20 02 00`**。
3. 主机的 int 站点扫描把 `CD 20` 改写成 `CC 90`（白名单里有 `0x20`！）⇒ 变成
   `E8 CC 90 02 00` ⇒ **调用目标 0x34894 → 0x3B893**（`AIL_install_timbre` 中部）。
4. 用脚本把 obj0 里全部 `CD xx` 候选与指令边界对照：112 个里只有这 1 个不是真 `int`。

**修法**：`dos.c` 不再改写任何游戏代码。`src/probe4.c` 实测出 ring3 执行 `int NN` 的异常语义
（`EXCEPTION_ACCESS_VIOLATION`，EIP 指向该指令；`int 3` 是 `EXCEPTION_BREAKPOINT`），VEH 于是直接
从 `[EIP] == 0xCD` 读向量并按"前缀 + 2 字节"跳过 —— 快照与反汇编器镜像逐字节一致，不再有
"哪条指令被我改坏了"这类问题。

**回归测试手段**：`--autokey=<延时ms:VK[,VK...];...>` 把按键按计划 PostMessage 给游戏窗口
（VK 名：`RETURN/SPACE/UP/DOWN/LEFT/RIGHT/...`）。走一遍
`--autokey=5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN` 就能不靠人复现整条 continue 路径：

```
dos: open 'FD2.SAV'        ← 进入"读取存档"
dos: open 'FD2.TMP'
（无 exception，运行到 watchdog 结束，1280 帧）
```

`--screenshot=<file.bmp> --shot-frame=<n>` 抓实际送显的帧可以看到已经进入剧情画面（王座厅 + 对话框），
即"continue 之后能正常玩"。

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

日志关键三行：`dos: open 'FD2.TMP' -> FFFFFFFF (2)` → `dos: create 'FD2.TMP' -> … (dos handle 5)`
→ `dos: open 'FD2.TMP' -> … (0)`，文件尺寸与原版一字不差。

同轮还顺带验到两件事：

- **游戏退出路径已通**（`§7.6` 的一项打勾）：`dos: INT10 set video mode 0x03` →
  `dos: INT 21h AH=4Ch terminate, code=3` → `ail: shutdown` → `dos: game requested exit`。
- **画面无回退**：`build/regress.bmp`（第 900 帧）仍是王座厅 + 对话框，调色板与通道序正确。
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

## 13. 显示/跨平台后端决策：**sokol**（0 DLL，各平台原生）（2026-10-05）

**需求重定义**（用户澄清）：要的**不是 D3D11**，只是**替换掉默认的 GDI 渲染**；
因此选 **sokol**（`sokol_app`+`sokol_gfx`+`sokol_audio`，单头文件、**0 DLL**，
Win=D3D11 / mac=Metal / Linux=GL）。SDL2/SDL3 的实测降为**备选记录**（§13.2、§13.3）。
**不用 git 分支分平台**，改用“单代码库 + 后端选择”。备选方案与实测数据见下。

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

### 13.2 SDL2 实测（`build/sdlprobe*.c`，SDL2 2.32.8 **x86**，宿主是 32 位进程）

| 项 | 实测值 |
|---|---|
| `SDL2.dll`（x86） | **1,338,880 B ≈ 1.28 MB**（x64 是 1,576,448 B） |
| Windows 可用渲染后端 | `direct3d`、**`direct3d11`**、`direct3d12`、`opengl`、`opengles2`、`software` |
| **SDL2 默认加速后端** | **`direct3d` = D3D9，不是 D3D11** ⇒ 必须显式指定，否则“用了 SDL 就没用上 D3D11” |
| `SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11")` | ✅ **一行生效**：日志 `renderer in use: direct3d11` |
| W3 每帧 CPU 成本 | `UpdateTexture(320×200 ARGB)` = 0.136 ms；`+Clear+Copy(→960×600)` = **0.092 ms**；`+Present` = 0.086 ms（3000 次取均值） |
| 加 `SDL_RENDERER_PRESENTVSYNC` | 6.24 ms/帧（vsync 等待；隐藏窗口下不是满刷新率，仅供量级参考） |
| 32 位链链 | ✅ `lib/x86/SDL2.lib` 存在，probe 编译/运行均通过（`sdlprobe*.exe`，留在 `build/`） |

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
3. **`audio_sokol.c`**：sokol_audio 回调流式替换 `waveOut + Sleep(200)`，
   并修 §12 提到的“音量在 synth 后端不生效 / 渐变未实现”。
4. **POSIX**：Linux（`libX11-dev` + GL 后端 + `platform_posix.c`：sigaction/mmap/pthread），
   `int NN`/`in out` 的信号语义用 `probe4.c` 的方法在目标机重测 → 首个非 Windows 产物。

