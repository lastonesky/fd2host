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

- **第 15 轮（2026-10-05）：FDPS 的 AIL 入口表 + 定时器族** —— FDPS 自己的 90 个 AIL 入口
  从 IDA 库的 trace 串 + 全量 `call` 扫描重建（`re/fdps_ail_patchset.csv`），宿主新增
  **1 ms tick 的定时器线程**直接调 guest 回调，**动画时钟精确 25 Hz**（`fire #100 at +4000 ms`），
  标题不再卡第一帧；同轮补齐 FDPS 用到而 FD2 没用到的 4 个样本入口、采样句柄池 4→8。
  FD2 回归 **8/8 PASS**。**下一道关口是 `INT 21h AH=4B`（EXEC，spawn `FD.EXE`）**。详见 **§15**。

- **第 16 轮（2026-10-05）：`INT 21h AH=4B`（EXEC）打通** —— 宿主把 FDPS 标题流程的
  `spawnlp(0,".\FD.EXE",…)` 实现成“**再拉一个 `fd2host.exe` 并等它退出**”，命令行尾巴按
  DOS 的 `[len][chars][0x0D]` 读进子进程 PSP:0x80，另补 `AH=4D`（取子进程退出码）、
  `--log=`（子进程单独日志，否则会截掉父日志）与**低内存串指令模拟**（`rep scasb` 扫尾巴）。
  实测：子进程跑起 `FD.EXE`、设 13h 模式、尾巴逐字节正确；**FD2 回归 8/8 PASS**。
  **新卡点：`FD1.Aud`/`FD1.Vid` 在整个 FDCollection 都不存在 ⇒ FD.EXE `exit(8)`**（空文件也不行）。详见 **§16**。

- **第 17 轮入口调研（只读）**：`FD.EXE` = **过场动画播放器**（`main` 把 `argv[2]` 整个读成音轨、
  解析 `argv[1]` 成画面对象逐帧 blit），缺的 `FD1.Vid`/`FD1.Aud` **整个合集都不存在**（空文件也 `exit(8)`）；
  父进程 spawn 后确实进了标题菜单（帧 150 = 23 色 → 帧 400 = 82 色），但**再无变化** ——
  根因是**游戏自己挂了 INT 9**（`sub_56560`/ISR `sub_565A7`，队列 `byte_7000F[10]`），
  宿主从不投递硬件中断 ⇒ 菜单永远读不到键（autokey 实测无效）。做法见 **§17**。

- **第 18 轮（2026-10-05）：INT 9 投递打通** —— 在**跑 guest 代码的线程**上压真正的中断帧注入
  handler（在宿主线程上跑会因异常帧残留崩）；修掉 **`type 0x02` fixup 写 4 字节**这个第 14 轮就有的
  加载器 bug（它把 ISR 的 `mov ds,eax` 改成了 `pop es`）。实测：标题菜单按 START NEW GAME
  **直接进到游戏内场景**（两张截图 + 直方图对比），FD2 回归 **8/8 PASS**。**新卡点：场景里读完
  `FACE.CEL` 后跳到 `EIP=0x1FFFC`（解引用 `0x43B4` 这个低于 64 KiB 的地址）**。详见 **§18**。

- **第 19 轮（2026-10-05）：源码转译开工 —— RLE 模块**：按用户决定**停止 FDPS 支持**（§7.8 已冻结），
  回到初始目标“逆向为高级语言代码”。首个模块 **RLE 解码**（`0x4E98D` 三模式 + `0x4E8D3` LUT 模式，
  含 token 流格式与颜色映射完整语义）翻译为 `src/game/rle.c`；新增**机器码对拍测试** `rlecheck`
  （LE 加载器映射原始 exe → 直接调原函数 vs 转译 C，随机合法流 **1900 例逐字节一致**，
  含全局副作用），FD2 回归 8/8 PASS。详见 **§19**。

- **第 20 轮（2026-10-05）：回归提速 75 s → 15 s** —— 用户反馈“跑测试结尾至少干等 10 秒”。
  实测：`FD2.TMP` 在 **11.7 s** 就写满（路径完成），而宿主固定跑满 60 s、脚本再盲睡 +15 s。
  宿主新增 **`--exit-when-file=<path>:<minbytes>`**（文件写满 + autokey 完成 + 2 s 缓冲 → 干净退出，
  退出前抓最后一帧作证据），`regress.ps1` 改为**轮询进程退出**并加入环境性崩溃**自动重试**。
  实测连续两次 **15 s / ALL PASS 8/8**。详见 **§20**。

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
   资源加载（`sub_111BA`）、RLE 解压/blit（`sub_4E98D` ✅ **第 19 轮已转译 + 机器码对拍**，见 §19）、
   脚本 VM（`sub_15F84`）、游戏工具库（0x4DED4..0x4EF29），最终产出可编译的 x86-64 引擎。
   逆向工作台与测绘起点见 `port/re/RE_MAP.md` + `port/re/funcmap.csv`（§10）。
5. **继续玩**：现在能进剧情画面了，下一批要验证的是战斗/地图等更深路径
   （`--autokey` 走不同的按键序列 + `--screenshot` 逐帧核对）。
6. **稳定性长跑**：连续运行 5 分钟以上与反复重启验证（游戏退出路径已验：`INT10 mode 3` →
   `AH=4Ch` → `ail: shutdown`，见 §12.4）。
7. ~~**文件写入 / 存档路径**~~ **平台侧已补完**（§12）：`AH=3C/41` + `AH=40 CX=0` 截断，
   `regress.ps1` 回归 8/8 PASS。**剩余**：“新游戏 → 首次存档 → `FD2.SAV` 从无到有”与
   “存档变小后的截断对拍”两条还没实测；`AH=49/4A` 仍是空操作（账本只增不减）。
8. ~~**FDPS（炎龙外传）跑起来**~~ **已冻结（2026-10-05 用户决定：不再继续支持 FDPS）** ——
   已达成的成果保留存档（§14 首跑、§15 AIL 定时器、§16 EXEC、§17 入口调研、§18 INT9：
   标题菜单已能按键进到游戏内场景，`build/fdps_menu2.png`）；宿主的通用能力（`--exe`、FDPS 的
   90 条 AIL 表、定时器线程、INT9 注入）保留在代码里不再主动维护，FDPS 的后续卡点
   （`FACE.CEL` 跳飞 §18.5、`FD1.Vid`/`FD1.Aud` 缺失等）不再投入。资源全部转向 FD2 源码转译。

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

34. **`VirtualAlloc` 的分配粒度是 64 KiB，基址向下取整**（第 14 轮）：想在 `0x71000` 开窗口，
    Windows 实际落到 `0x70000`，撞上游戏对象就报 **ERROR 487**。低内存镜像必须 **64 KiB 对齐**
    （`dos_choose_lowmem()`），段对齐不够。

35. **GUI 子系统没有 fd 0/1/2，`freopen` 又不更新 `STD_*_HANDLE`**（第 14 轮）：宿主把 `stdout`
    重定向到 `host.log` 后，`GetStdHandle(STD_OUTPUT_HANDLE)` 仍无效 → 游戏经 `AH=40h` 写句柄 1
    得到 **0 字节 / 错误 6** → **游戏自己的 printf 全部丢失**（日志里只看得到
    `dos: write h=1 want=39 n=0`）。修法：`_dup2(_fileno(stdout), 1)` 把 fd1/fd2 钉到日志上，
    `files_init()` 改用 `_get_osfhandle()` 取真实句柄。**看不到游戏文本 ≠ 游戏没报错**。

36. **LE 末对象在磁盘上的字节数 ≠ `vsize`**（尾部 BSS 不落盘）（第 14 轮）：
    `data_start = EOF − Σ跨度` 对 FD2 成立，对 FDPS 差 **0x1F 字节**（`vsize=0x54` 只落 `0x35`），
    整个映像**错位 31 字节** → 执行的是错位指令流（`mov es,[ebx]`、`EBX=0`，且 `int` 服务数=0）。
    正确来源是 LE 头 **`+0x2C` = 末页实际数据字节数**（FD2 `0x4D2`、FDPS `0x35`），两者同时成立。
    **判据**：拿 IDA 在入口 `0x43008` 处的字节与两种候选起点的文件内容对拍（`re/preflight.py` 思路）。

37. **fixup 类型不止 `0x07`**（第 14 轮）：FDPS 第 71 页用了 **`0x02`（5 字节：`02 00 src:2 obj:1`，
    无目标偏移字段）**，旧代码遇未知类型就 `break` → **该页剩下 987 字节（140 条 fixup）全丢**。
    新文件必须先跑 `re/fixup_scan.py`，看到 `bad=0 / leftover=0` 才算解析干净。

38. **VGA 状态口 `0x3DA` 的位必须会变**（第 14 轮）：`bit3`=垂直回扫是**状态位**，恒返回 `0x09` 会让
    “等回扫开始 → 等回扫结束”这对经典写法**永远退不出**（FDPS 标题循环正是如此）。现象很有辨识度：
    画面永远停在第一帧 + **端口操作数暴涨到几千万次**（死循环在狂读状态口）。现在每次读翻转 `0x09↔0x00`。

39. **CD 检测 = `INT 2Fh AX=1500h`（MSCDEX），看的是 BX**（第 14 轮）：FDPS `sub_3C3A6` 调
    `int386(0x2F,{AX=0x1500})` 后只判 `BX==0`，为 0 就打印 `Fatal error: CDROM is not install!!!`
    并 `exit(1)`。宿主现在回 `AL=FFh, BX=0x0210`（2.10）。

40. **`AH=43h`（取/置文件属性）被 CRT 的 `access()` 用到**（第 14 轮）：FDPS 启动第一件事就是
    `access("DISK.NO", 0)`，未实现时 `CF=1` → 游戏当“文件不存在”直接 `exit(1)`。
    FD2 从不用这个功能 —— **“FD2 没用到”不等于“别的游戏也不用”**，新增游戏前先跑 `re/preflight.py`。

41. **`VirtualAlloc(MEM_COMMIT)` 不能跨越多个预留区域**（第 14 轮，自己修自己引入的坑）：
    把早期预留拆成逐个 64 KiB 块后，每个块是**独立区域**；再对 `0x10000+0x3F000` 做一次性 commit
    就被拒（**487**），即使每一页都已 COMMIT —— 判据是 `VirtualQuery` 看到 `region_size=0x10000`
    （而不是合并后的大区域）。修法：**按区域逐段 commit**（`le_commit_range()`，`le.c`/`dos.c` 共用），
    遇到 FREE 子块先 RESERVE。另注意：早期预留失败的提示只能在 CRT 起来后打印（`fd2_entry` 里不能用
    stdio），所以 **原因与报错往往不在同一行** —— 早预留的掩码写在 `stderr`（`host.err`）。

42. **IDA 会把相邻函数并成一个，trace 串反查出的“函数地址”不一定是入口**（第 15 轮）：
    FDPS 的 `AIL_start_all_timers()` 那条 printf 落在 `AIL_start_timer` 的函数体内（`0x3E323`），
    `AIL_release_sequence_handle` 同理并进了 `0x403ED`；按 `get_func(ref).start_ea` 反查会把
    **别名指到前一个函数**。**真入口以 `call`/`jmp` 目标为准**（全量扫完：AIL 公共区 52 个 call
    目标 = 47 个公共入口 + 5 个内部工具），别名只用于给补丁表起名字。另：打 5 字节 `jmp` 前
    **必须做两两间距 ≥5 字节的检查**（90 个地址实测 0 处冲突）。

43. **宿主的采样句柄池要够大，且 shutdown 必须释放**（第 15 轮）：FDPS 一次性
    `AIL_allocate_sample_handle` × 8（FD2 只要 2 个），池 = 4 时第 5 个开始打
    `out of handles`；而 FDPS **在同一个进程里会先 `AIL_shutdown` 再重新 init**（spawn 回来后
    `sub_30CB0 → sub_30270(25)`），不释放的话第二次连一个句柄都拿不到 ⇒ 音效彻底消失。
    现在池 = 8，`host_AIL_shutdown` 把 sample/seq 句柄全清零。

44. **`AIL_sample_status` 必须回 `4` 才算“空闲”**（第 15 轮，值来自 FDPS 自己的 DIG 驱动）：
    游戏用 `status == 4` 找空闲句柄（`sub_303C0`/`sub_30790`）并判断“播完了”（`sub_304D0`），
    原版写入点 `mov dword [h+4], 1/2/4/8`（`re/fdps_digcore_*.c`：1=播中、2=循环中、4=空闲、8=停止）。
    回 `0` 或其它值会让游戏认为句柄全忙 ⇒ 8 个句柄用完后**再也不播音效**。

45. **DOS 的命令尾巴不是 C 字符串**（第 16 轮）：`INT 21h AH=4B` 参数块里那个指针指向的是
    **`[len][chars][0x0D]`**（PSP 格式），不是 NUL 结尾的串。按 C 串读会把长度字节和后面的
    栈垃圾一起带走（实测读到 125 字节垃圾，再原样写进子进程 PSP:0x80）。
    正确读法：`n = p[0]; if (p[1+n] == 0x0D) tail = p[1..n]`（`guest_cmdtail()`）。

46. **低内存模拟要支持串指令**（第 16 轮）：CRT 解析命令尾巴用 `mov cl,es:[di-1]` + **`rep scasb`**。
    前者 `emulate_lowmem_access()` 已能单步跳过，后者一个指令要碰几十次低内存，
    VEH 报 `unmatched low-memory access` 就直接崩（FD.EXE 子进程首发）。新增
    `emulate_lowmem_string()`：把 `A4..AF`（movs/stos/lods/cmps/scas）整条在宿主侧跑完，
    按 ZF/ECX/方向位维护语义再跳过指令。**尾巴为空时永远碰不到这段**（FD2 就是），
    所以“FD2 没事”不代表新游戏没事。

47. **子进程会把父进程的 `host.log` 截掉**（第 16 轮）：`freopen(log,"w",stdout)` 对同一个文件
    再开一次 = 把父日志清空。所以新增 `--log=<path>`，`AH=4B` 给每个子进程发
    `host.<pid>.log`；`--log` 必须在**重定向之前**扫描 argv（参数归一化发生在重定向之后，
    两种写法都得手动认）。

48. **低地址窗被进程初始化阶段的映射抢走（偶发，两种签名）**（第 16 轮发现，第 20 轮补第二签名）：
    预留发生在 `fd2_entry`（DllMain 之后、CRT 之前），抢不回来，重跑即好（新 ASLR 布局）——
    - **签名 A（0x10000）**：`le: cannot reserve object region @0x10000: 487` + `type=MAPPED
      prot=0x2`，是某个 DLL 在 DllMain 阶段建的只读文件映射。失败路径会用
      `K32GetMappedFileNameA` 打出**是谁**。
    - **签名 B（0x90000..0xFFFFF，第 20 轮实测）**：`host.err` 出现
      `le: guest window blocks 0x7F00 not reserved (the loader put something here)`
      （mask 位 8..14 = 0x90000..0xFFFFF 没抢到）+ `cannot commit @0x90000/@0xC0000 (87)` ⇒
      **VGA 窗口缺失**，渲染线程转换帧缓冲读到 `0xAD000`（= 0xA0000+0xD000）即 AV，
      **EIP 报在宿主映像的像素转换循环里**（症状很误导，像是宿主自己跳飞）。
      判据：`host.err` 的 mask 行 + `host.log` 的 `cpu:` 行。regress.ps1 见此签名**自动重试**
      （最多 3 次）；真实回归没有该签名，首跑失败即报。

49. **`type 0x02` fixup 的源只有 16 位，写 4 字节会踩掉后面 2 字节代码**（第 18 轮，第 14 轮引入）：
    FDPS 唯一一条 `0x02` 记录指向 `mov ax,seg X` 的 imm16（2 字节），当时的处理写成了
    “4 字节对象基址” ⇒ 把下一条指令 `8E D8`（`mov ds,eax`）改成了 `07 00`（`pop es`），
    INT 9 handler 从那里开始**指令流错位**：多出的 `PUSHA` 吃 32 字节 → `pop ds` 弹垃圾 → #GP。
    判据：**文件 / IDA / 运行时三份字节对照**（§18.2）+ 单步日志看每条指令的 ESP 增量。
    修法：写 2 字节，值 = 本进程的平坦数据选择子（`mov sel,ds`）。

50. **guest ISR 不要在宿主线程上跑**（第 18 轮）：`pushfd/push cs/call` + 依赖它的 `iret` 的写法
    会在宿主线程上留下 **28 字节没回收的异常帧**（`sti`/`in` 的 PRIV 异常），`pop ds` 因此 #GP；
    而同样的异常在跑 guest 代码的线程上完全配平。做法：**在 VEH 里压真正的中断帧**
    （`Esp-12` 写 `[EIP][CS][EFLAGS]`、`EIP=handler`），handler 的 `iret` 天然弹回被打断的指令。

51. **游戏自己挂了 INT9 就不能再写 BIOS 环形队列**（第 18 轮）：真机上游戏替换了 BIOS 的键盘
    处理器且不链回 ⇒ `0x41E` 环是空的；宿主两条路都写 = **一次按键给两次**，菜单多走一格后
    跳进没填好的表（`EIP=0x1FFFC`）。`dos_deliver_key()` 返回“游戏已接管”时 `host_key` 直接 return。

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
| 抓当前帧画面（不依赖窗口/桌面） | `--exit-after=30 --screenshot=E:\FD2\port\build\frame.bmp --shot-frame=700`，日志出现 `host: frame 700 dumped` 后把 BMP 转 PNG 查看（`[System.Drawing.Image]::FromFile`）。帧数见 watchdog 行 `(N frames drawn)`。帧内容也可用 ASCII 网格打印（不依赖看图工具）：对 `GetPixel` 采样 64×24、按亮度映射成 ` .:-=+*#%@` |
| **换一个游戏前的静态体检** | `python re\preflight.py <exe>`（LE/对象布局/与宿主预留区冲突/AIL 特征/扩展器）+ `python re\fixup_scan.py <exe>`（fixup 语法，要 `bad=0 leftover=0`）。两个都不运行、零风险，能在开跑前报出必修点（§14.1） |
| 看不到游戏自己的文本 | 先看日志里 `dos: write h=1 ... n=` 是不是 0（句柄无效 = 游戏 printf 全丢，§8-35）；`AH=3F/40` 对 ≤512 字节的小传输有内容日志（前 40 条） |
| 游戏停在第一帧 / 端口操作数暴涨 | `0x3DA` 状态位没翻转（等回扫的经典写法死循环，§8-38）；端口操作数是正常量级的百倍/千倍即是此病 |
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
| 第二个数据库 | `E:\Games\FDCollection\Game\FDPS\FDPS.EXE.i64`（2026-10-05 由 ida MCP `open_database` 自动分析并 `save_database` 生成，**无需手动在 IDA GUI 里加载**：1353 函数、437 串、入口 `start`=0x43008，与 `preflight.py` 一致） |
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

## 13. 显示/跨平台后端决策：**sokol**（0 DLL，各平台原生）（2026-10-05）

**需求重定义**（用户澄清）：要的**不是 D3D11**，只是**替换掉默认的 GDI 渲染**；
因此选 **sokol**（`sokol_app`+`sokol_gfx`+`sokol_audio`，单头文件、**0 DLL**，
Win=D3D11 / mac=Metal / Linux=GL）。SDL2/SDL3 的实测降为**备选记录**（§13.2、§13.3）。
**不用 git 分支分平台**，改用“单代码库 + 后端选择”。备选方案与实测数据见下。

> **2026-10-05 清理**：定案 sokol 后，`build/` 下的 SDL2/SDL3 全部产物已删除
> （`sdlprobe*`/`sdl3probe*` 源码与 exe、`SDL2.dll`/`SDL3.dll`、`SDL3-devel.zip`、`build/sdl3/` 解压目录、
> 仅服务于 SDL 依赖查看的 `deps.bat`）。§13.2/§13.3 的实测数据**留档**；如需重测按当时流程重写 probe。

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

### 13.2 SDL2 实测（probe 已删，数据留档；原 `build/sdlprobe*.c`，SDL2 2.32.8 **x86**，宿主是 32 位进程）

| 项 | 实测值 |
|---|---|
| `SDL2.dll`（x86） | **1,338,880 B ≈ 1.28 MB**（x64 是 1,576,448 B） |
| Windows 可用渲染后端 | `direct3d`、**`direct3d11`**、`direct3d12`、`opengl`、`opengles2`、`software` |
| **SDL2 默认加速后端** | **`direct3d` = D3D9，不是 D3D11** ⇒ 必须显式指定，否则“用了 SDL 就没用上 D3D11” |
| `SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11")` | ✅ **一行生效**：日志 `renderer in use: direct3d11` |
| W3 每帧 CPU 成本 | `UpdateTexture(320×200 ARGB)` = 0.136 ms；`+Clear+Copy(→960×600)` = **0.092 ms**；`+Present` = 0.086 ms（3000 次取均值） |
| 加 `SDL_RENDERER_PRESENTVSYNC` | 6.24 ms/帧（vsync 等待；隐藏窗口下不是满刷新率，仅供量级参考） |
| 32 位链链 | ✅ `lib/x86/SDL2.lib` 存在，probe 编译/运行均通过（`sdlprobe*.exe`，已随清理删除） |

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
3. **`audio_sokol.c` + 抽出 `audio.h`**：sokol_audio 回调流式替换 `waveOut + Sleep(200)` 轮询，
   让 `ail.c`/`synth.c` 只见接口；并修 §12 提到的“音量在 synth 后端不生效 / 渐变未实现”。
4. **POSIX + `platform.h`**：抽 OS 适配层（内存/线程/文件/异常），Win32 实现进
   `platform_win32.c`（VEH 暂时仍留 `dos.c`，语义转换见下），POSIX 实现进 `platform_posix.c`
   （`sigaction`/`mmap`/`pthread`）；Linux 侧需要 `libX11-dev` + sokol GL 后端。
   `int NN`/`in out` 的信号语义用 `probe4.c` 的方法在目标机重测 → 首个非 Windows 产物。

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
   与 FD2 当年建 52 条表的手法相同（`re/RE_MAP.md` §3、`PROGRESS.md` §10）。
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

---

## 19. 第 19 轮：源码转译开工 —— RLE 模块（0x4E98D/0x4E8D3）+ 机器码对拍（2026-10-05）

**背景**：用户决定**停止 FDPS 支持**（§7.8 已冻结），全部资源回到初始目标——逆向 FD2 为
高级语言源码（§7.4 “逐步源码化”正式开工）。本轮交付：**第一个转译模块 + 可复用的对拍方法**。

### 19.1 模块测绘（ida MCP，产物都在 `re/`）

RLE 解码位于游戏自带工具库（0x4DED4..0x4EF29）内，家族成员：

| 地址 | 大小 | 身份 | 状态 |
|---|---|---|---|
| `0x4E98D` | 443 | **RLE 解码 · 三模式**（流内色 / 平涂 / 8 色 ramp） | ✅ 本轮转译 |
| `0x4E8D3` | 186 | **RLE 解码 · 调色板 LUT 变体**（`color = table[c]`，a6=表指针） | ✅ 本轮转译 |
| `0x4EC7C`/`0x4ECA4` | 40/27 | 无压缩块解码（读 [w][h] 头后逐行 memcpy；**usercall 寄存器传参** edi/esi/ebp） | 下一批 |
| `0x4ECBF`/`0x4ECF0` | 49/27 | 无压缩块打包（上者的逆操作） | 下一批 |
| `0x4EB48` | 17 | `(&off_627D8)[a1]` 函数指针表访问器 | 待归类 |
| `0x4ED7A` | 188 | 16×16 字形渲染（1 位/像素 + 前景/阴影双色 + 可选底色填充） | 下一批 |

全局状态 `word_627B4/word_627B6`（0x627B4/0x627B6）的全部 xref 都落在 0x4E8D3..0x4ECF9
⇒ 是本家族私有的“当前图宽 / 剩余行数”（宿主崩溃转储读它做诊断，§6 第 4 条）。

### 19.2 流格式与语义（从反汇编钉死，`re/sub_4E98D.disasm`）

头：`[u16 width][u16 height]`，随后 token 流连续跨行消费，每行末尾目的地跳过 `pitch - width`
（行回车）。token = `[2-bit 类型 | 6-bit count-1]`：

| token | 行为 | 流内字节 |
|---|---|---|
| `00xxxxxx` | 实心 run：count 个连续像素 | +1 颜色字节 |
| `01xxxxxx` | **隔位 run**：写在奇数位置，消耗 2×count 像素 | +1 颜色字节 |
| `10xxxxxx` | 字面量：count 个原始像素 | +count 字节 |
| `11xxxxxx` | 跳过：count 个透明像素 | 无 |

三种颜色模式（`rle_decode` 的 a6，**流的消费方式在所有模式下完全一致**，只影响写入值）：
`-1` = 流内原色（已验证调用点绝大多数是它）；`(u16)a6 <= 0xFF` = 平涂 `(u8)a6`（剪影）；
其余 = 8 色 ramp `(u8)a6 + (((u16)a6 >> 8) + c) & 7`（**实用调用点未确认，按字节语义保留**）。
`rle_decode_lut`（0x4E8D3）= `table[c]` 查表。副作用：进函数写宽/高全局，结束时剩余行数递减到 0。

调用点实证（`re/rle_args_*.txt`，39 + 4 个站点）：

- 已确认的模式几乎全是 `-1`；典型用法（`sub_2F4D4` 反编译）：
  `sub_4E98D(BG.DAT 资源缓冲, 0, 50, dst, 640, -1)`。
- 4 个看起来神秘的 `push dword_5413F[edx*4]` 站点：`dword_5413F[i]` =
  `sub_111BA("BG.DAT", i)` 返回的**资源缓冲指针**，在调用序列里是 **src（a1）不是模式** ——
  靠 `sub_2F4D4` 反编译解开（静态 push 扫描容易把实参序搞反，教训）。
- `sub_4E8D3` 的 4 个调用点 a6 传缓冲区/表指针（LUT）✓。

### 19.3 转译产物

| 文件 | 内容 |
|---|---|
| `src/game/rle.h` | 格式文档（token 表、模式语义、全局副作用）+ API |
| `src/game/rle.c` | `rle_decode`（原 0x4E98D）+ `rle_decode_lut`（原 0x4E8D3）；两者共用一条 token 状态机（原版是两份手写副本） |
| `src/rlecheck.c` | **机器码对拍测试**（build.ps1 新目标 `rlecheck`） |
| `re/sub_4E98D.c/.disasm`、`re/sub_4E8D3.c`、`re/sub_4EC*.c`、`re/sub_4ED7A.c`、`re/sub_2F4D4.c` 等 | IDA 反编译/反汇编存档 |

### 19.4 对拍方法（可复用于后续每个模块）

`build\rlecheck.exe`：用 LE 加载器把 FD2.EXE 映射成 RWX（与 letest 同一条可信加载路径），
**通过函数指针直接调用原始机器码**，与转译 C 解码同一批随机流，断言：

1. 目标缓冲逐字节一致；
2. 副作用一致（原版读 0x627B4/0x627B6 vs 转译版 `rle_width/rle_height`）。

随机流是**合法**生成器（每行像素精确填满、4 类 token 全覆盖）；覆盖 10 个模式值
（-1、flat 0/0x37/0xFF、ramp 0x100/0x407/0x1234、`(u16)0x37` flat 边界、0xFFFFFF、0xFFFFFFFF）
+ LUT 模式 400 例。

### 19.5 实测判据

```
build\rlecheck.exe  →  PASS: 1900 cases, 0 failures（首轮即通过）
regress.ps1         →  ALL PASS 8/8（build.ps1 新目标未影响宿主）
```

### 19.6 下轮入口

1. 下一批转译：无压缩块族（`0x4EC7C`/`0x4ECBF`，注意 **usercall 寄存器传参**，对拍需要
   `__asm` 寄存器 thunk 设 edi/esi/ebp）、字形渲染 `0x4ED7A`、工具库其余函数
   （`re/funcmap.csv` 中 0x4DED4..0x4EF29 段约 60 个）。
2. ★★★ 级：资源加载器 `sub_111BA`、脚本 VM `sub_15F84`（对拍同法，但要先抽清全局状态区）。
3. 模块归类（RE_MAP §5 阶段 1）继续：`lib_nosym` 剩余、`gfx_A0000` 名单精化。
4. FDPS 相关工作全部冻结（§7.8）。

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

---

## 21. 第 21 轮：官方逆向知识库（`docs/`）评估 + 图形 blit 工具族转译（2026-10-05）

### 21.1 `port/docs/` 是什么、能不能直接用

`port/docs/` 是 `github.com/wicanr2/fd2_re`（同游戏的 Go/Ebiten **重制**项目）的 docs 快照：
845 个文件 / **28 MB**，`knowledge-base/` 78 份主题文档 + `data/`（IDA 转储、覆盖矩阵、
逐章证据、UI traces、`fd2_function_inventory.json` 等）。**对本项目有用**：函数级语义
（如 `sub_15F84` 文本渲染器、`sub_111BA` 资源加载器）、调用点实参、数据结构与各 U I 资源
的用途，能显著省掉重新摸索的时间。检索用 `rg`，批量结论写文件、不要灌进上下文。

⚠ **重大差异（必须记住）**：docs 分析的 FD2.EXE 是**另一个 build**：

| | docs 的 build | 本项目在跑的 build |
|---|---|---|
| 大小 | 357074 B | **509158 B** |
| md5 | `b97caf2239a27a896069d03549d96e1e` | **`a6e341a8decc6ebf7f4872076d9cf161`** |
| 映像 | `E:\Games\FDCollection\Game\FD2\FD2.EXE` | `E:\FD2\FD2.EXE`（`FD2.EXE.i64` 记录同一 md5） |

- 大函数边界/地址高度重合（`sub_15F84`=0x15F84/1380 B、`sub_111BA`=0x111BA/235 B 在两边
  起止与大小**完全一致**），所以 docs 的高层语义基本可直接参考；
- 但**部分立即数与 call 目标不同**（实证：`0x15FC4` 的比较立即数、`0x165AC` 的 call 位移
  `0x36CD2` vs `0x3702A`），且 `0x4E8xx` 图形族布局不同 —— docs 的 `sub_4EA2A`（字形渲染）
  在本 build 是 `0x4ED7A`；本 build 的 `0x4E98D` 是 443 B 的 RLE，直接覆盖 docs 所谓的
  `0x4E8xx..0x4EAxx` 区（docs 的 `sub_4E8AF`/`sub_4E8E1`/`sub_4E92C`/`sub_4E96F` 在本
  build 对应的是 `0x4E8A5`/`0x4E8BC`/… 的另一批函数）。
- **结论/约定**：docs 只作语义线索；任何**地址 / 常量 / 指令字节**必须以本 build 的
  `E:\FD2\FD2.EXE.i64`（ida MCP）复核。`docs/` 为 28 MB 外部快照，加入 `.gitignore`，不入库。

### 21.2 转译：图形 blit 工具族 `src/game/gfx.c`

§19.6 点名的"下一批"其实是一个自洽的 obj0 工具库（0x4EC7C..0x4EEE0），全部是**纯内存
操作**（目标/来源指针都由参数给出），因此可以像 RLE 一样用"机器码对拍"验证。本 build 的
六个入口（初始 IDA 只把一半识别成独立函数，usercall 半截函数要用别名/手工对齐）：

| 转译名 | 原地址 | 作用 | 原传参 ABI |
|---|---|---|---|
| `gfx_save_rect` | `0x4ECBF`+`0x4ECF0` | strided 曲面 → 紧凑矩形记录 `[u16 w][u16 h][i32 off][pixels]`（**保存**，取源 stride） | cdecl 6 参 |
| `gfx_restore_rect` | `0x4EC7C`+`0x4ECA4` | 记录 → `surface+off`（**恢复**，取目标 stride；off 是记录里的字段） | cdecl 3 参 |
| `gfx_blit_block` | `0x4ED0B` | `[u16 w][u16 h][pixels]` 不透明块 → strided 目标 | cdecl 3 参 |
| `gfx_blit_transparent` | `0x4ED34`+`0x4ED4F` | 同上，**0 字节透明**（不覆盖目标） | cdecl 3 参 |
| `gfx_draw_glyph` | `0x4ED7A` | 16×16 1bpp 字形：每行 u16（**先 xchg al,ah**），置位画前景，另在**下一行同列/左一列**画阴影；`fill!=0` 先平铺整格；**字形 index==10 跳过不画** | cdecl 7 参 |
| `gfx_expand_scanlines` | `0x4EEE0` | 192 行、每行从 `src+4+row*320+table[idx]` 复制 312 B 到 320-B 步进目标，`idx` 每行 +1 模 16 | cdecl 3 参 |

配套：`byte_627C8`（16 B scanline 相位表，**全镜像只有 1 个只读 xref**）已作为常量
`gfx_phase_table[16]` 嵌入；原 0x627A3..0x627B0 的一堆 scratch 全局以 `gfx_pen_*`/`gfx_rec_*`
镜像出来供对拍断言。细节语义与边界（16 位宽度/行计数、`gfx_blit_transparent`/`gfx_draw_glyph`
把 stride **截断成 u16**、`gfx_restore_rect`/`gfx_blit_block`/`gfx_save_rect` 用完整 32 位 stride）
见 `src/game/gfx.h`。

### 21.3 对拍（`src/gfxcheck.c` + `build.ps1 -Target gfxcheck`）

不同 usercall 内部实现（`0x4ECA4`/`0x4ECF0` 走 edi/esi/ebp 寄存器）**不用 `__asm` thunk**：
它们只被 cdecl 包装器调用，而包装器自己装寄存器，所以直接按 cdecl 调 `0x4EC7C`/`0x4ECBF`/
`0x4ED34` 即可。测试用与 `rlecheck` 同一可信 LE 加载路径（映射 FD2.EXE 为可执行、应用 fixup），
对每类随机用例同时跑原机器码与 C 实现，断言：

1. 目标缓冲**含哨兵余量**逐字节一致（能抓到越界/未写的字节）；
2. 原 scratch 全局（0x627B4/B6/A3/A5/A6/A7/AC/B0）与镜像 C 全局一致；
3. 嵌入的 `gfx_phase_table` 与镜像 `byte_627C8` 一致。

覆盖：save↔restore 往返 300、非透明块 300、透明块 300（约半数 0 字节）、字形 400
（含 `fill=0` 与 `index=10`）、scanline 重排 150。

### 21.4 实测判据

```
build\gfxcheck.exe  →  PASS: 1450 cases, 0 failures（首轮即通过）
build\rlecheck.exe  →  PASS: 1900 cases, 0 failures（回归，未受影响）
regress.ps1         →  ALL PASS 8/8（fd2host 未改，15 s）
```

### 21.5 下轮入口

1. `gfx_draw_glyph` 的消费方 —— 文本渲染器 `sub_15F84`（docs 有逐控制码语义可参照，
   但以本 build 复核）；先抽清它的 usercall 14 寄存器参数与全局状态区。
2. 资源加载器 `sub_111BA`（LMI 容器：`fseek(4*index+6)` 读 `{start,end}` → malloc → 读入，
   并释放旧指针）—— 可作下一个"叶子 + 对拍"目标，或直接接进宿主以替换原生 fopen 路径。
3. 图形工具库剩余成员（0x4DED4..0x4E866 绘图/数学原语）继续分类转译。
4. FDPS 相关工作仍冻结（§7.8）。

---

## 22. 第 22 轮：docs 知识库清理 + 24×24 精灵 RLE 族转译（2026-10-05）

### 22.1 `docs/` 清理（28 MB / 845 文件 → 9 MB / 274 文件）

按"对理解 FD2.EXE 是否有用"取舍（清单见 `docs/KEEP.md`）：

| 保留 | 删除（上游重制运营产物，可重新 clone 恢复） |
|---|---|
| `knowledge-base/` 67 篇 + `scene-decode/`（格式、函数语义、逐章 RE 证据） | `SESSION-HANDOFF`、`91-worklist*`、`99-reflections`（历史）、`18/38/41/60/61/96`（重制工程） |
| `data/ida/*.txt`（106 份原始 IDA 证据）+ `fd2_function_inventory.json`、`fd2_unknown_footprints.json` | `data/ida/*.json`（86 份逐章验证收据） |
| `data/exe_tables/*.json`（EXE 抽出的数据表）、`data/*.txt`/`*.md`（原始反汇编） | `data/ui-traces/`、`data/parity-plans/`、`data/parity-slots/`、`data/chapter_beats/` |
| `data/*.json` 21 份跨切面游戏数据（战斗事件/肖像/武器/商店/字形…） | `localization`/`video`/`schema`/`verification`、remake worklist/进度 JSON |

### 22.2 转译：24×24 精灵 RLE 族（`src/game/sprite24.c`）

obj0 里藏着**同一台 24×24 RLE 状态机的 7 份手写副本**（0x4DF84..0x4E29C），
差别只在"流内颜色字节 → 像素"的映射和"透明 token（type 11）"的行为。本 build 的
七个入口（RE_MAP §4 曾把它们笼统归为"24×24 图元"）：

| 转译名 | 原地址 | 颜色映射 | type 11 |
|---|---|---|---|
| `sprite24_ramp` | `0x4DF84` | `base + ((rot + c) & 7)` | 跳过 |
| `sprite24_pal_recolor` | `0x4E016` | `pal[c]` | **把目标已有像素经 pal 重新着色** |
| `sprite24_pal` | `0x4E0A2` | `pal[c]` | 跳过 |
| `sprite24_const` | `0x4E127` | `(u8)stride`（原版把 arg2 低字节当颜色！） | 跳过 |
| `sprite24_ramp24` | `0x4E1A6` | `(c & 7) + 24` | 跳过 |
| `sprite24_plain` | `0x4E22A` | `c` | 跳过 |
| `sprite24_plain49` | `0x4E29C` | `c` | **填 0x49** |

流格式与画面 RLE（`rle.c`）同构（`[2-bit type | 6-bit count-1]`），但固定 24×24、
每行填满才 `dst += stride - 24`；type10 是"逐像素读 1 字节并经映射"（plain 模式即逐字节拷贝）。
原版 7 份循环合并为一台引擎 + 模式结构体。原版入口都是 cdecl（无 usercall 寄存器参数），
可直接经函数指针调用；`sprite24_const` 的颜色确实取自 stride 的低字节（callers 传 `stride|color` 打包值）。

### 22.3 对拍（`src/sprite24check.c` + `build.ps1 -Target sprite24check`）

与 `rlecheck`/`gfxcheck` 同一可信 LE 加载路径；每个模式 300 例随机合法流（每行精确填满、
4 类 token 全覆盖）、随机 stride 24..300、随机 pal/base/rot；目标缓冲带哨兵余量逐字节比对
（recolor 模式两边用相同初值）。

```
build\sprite24check.exe  →  PASS: 2100 cases, 0 failures（首轮即通过）
```

### 22.4 新坑：对拍 exe 自己被 ASLR 放进 guest 窗口 → 低地址预留失败

`sprite24check.exe` 首次运行**必失败**：`le: cannot reserve object region @0x10000: 487`。
真因不是 kernel32，而是**对拍 exe 自己的映像被加载到 `0x30000`**（落在 guest 窗口
`0x10000..0x6FFFF` 内），于是 `le_reserve_address_space_early()` 的强制块被自己占掉。
`gfxcheck`/`rlecheck` 当时映像恰好在高处，所以没暴露。

- **诊断信息误导**：失败后打印的 `0x10000 ... PRIVATE prot=0x40` 是**我们自己已经成功预留的块**
  （`MEM_COMMIT|PAGE_EXECUTE_READWRITE`），不是冲突源；真正冲突块可能在 0x20000..0x6FFFF。
- **修复**：所有走 `le.c` 的 console 对拍目标（`letest`/`rlecheck`/`gfxcheck`/`sprite24check`）
  在 `build.ps1` 加 `/link /BASE:0x60000000`（与 `fd2host` 同一手法）。这与 §8-48 签名 B
  是同源问题；宿主另有 `regress.ps1` 自动重试兜底。

### 22.5 实测判据

```
build\sprite24check.exe →  PASS: 2100 cases, 0 failures
build\gfxcheck.exe      →  PASS: 1450 cases, 0 failures（回归）
build\rlecheck.exe      →  PASS: 1900 cases, 0 failures（回归）
regress.ps1             →  ALL PASS 8/8（fd2host 未改，15 s）
```

### 22.6 下轮入口

1. obj0 工具库剩余：纯字节/调色板变换 `0x4DED4/0x4DEEC/0x4DF09/0x4DF28/0x4DF4C`、
   表访问器 `0x4E7DD..0x4E8BC`、掩码重着色 `0x4E795`（都可续用对拍法）。
2. 连通性/BFS 簇 `0x4E390..0x4E751`（`byte_60068/69` 网格、四方向递归、写回 `dword_60073`）——
   **疑似战场移动范围/寻路**，价值高但有全局状态，转译前先归档数据布局。
3. ★★★ 未动：资源加载器 `sub_111BA`（LMI 容器；内部走 Watcom CRT 的 `fopen/fread`，
   独立对拍需先接宿主 `dos.c` 的文件服务，或改成对目录解析的纯函数）、文本/脚本渲染器 `sub_15F84`。

---

## 23. 第 23 轮：obj0 字节/调色板工具函数转译（2026-10-05）

### 23.1 转译：`src/game/util.c`

第 22 轮把 0x4DED4..0x4E8C0 工具库地形图勾出来了（RE_MAP §4），本轮清掉其中**纯函数**一批：

| 转译名 | 原地址 | 语义 |
|---|---|---|
| `util_rec3` | `0x4DED4` | `base + 3*index`（原版硬编码 base=0x60181；C 版把 base 参数化以便成为数据） |
| `util_translate` | `0x4DEEC` | 就地查表翻译：`buf[i] = table[buf[i]]` |
| `util_sum_tail4` | `0x4DF09` | 求和 `buf[0..n-5]`（原版真地忽略末尾 4 字节，`sub ecx,4`） |
| `util_deobfuscate` | `0x4DF28` | 就地滚动异或：`state=0xA5; state=rol16(state+0x9014,3); buf[i]^=state&0xFF` |
| `util_fix_records` | `0x4DF4C` | 头 `[u8 a][..][u8 b]`，count=a*b；每 4 字节记录：`+3=0xFF, +2&=0x1F, +1&=0x03` |
| `util_mask_recolor` | `0x4E795` | 头 `[u16 w][u16 h][w*h 掩码]`：掩码非 0 时 `dst = pal[dst]`，行步进 stride |

原版循环都是 x86 `loop`/`dec+jnz`（底测 do-while，0 计数会回绕 2³²/2¹⁶ 次），
C 版保留 do-while 形式；游戏数据不会出现 0 计数。`0x4E795` 写 `word_6017B`（C 版镜像为
`util_mask_w`）供对拍断言。

### 23.2 实测坑（都很典型）

1. **`0x4DF09` 改 EBX 却不保存**（其余五个都 `push ebx`）。查调用点：callers 只读 EAX 返回值、
   不依赖 EBX ⇒ EBX 是"死"寄存器，C 版无需复刻；但**对拍时**把它当普通 cdecl 调会让编译器
   的 EBX 状态被踩，`sum` 结果变垃圾。修法：utilcheck 用 `__asm { push ebx; ...; pop ebx }`
   包住原函数调用（`call_sum_orig`）。**将来把转译函数接回宿主时**，这种"原版不守 ABI"的
   函数要留意（我们的 C 版会正常保存 EBX，是安全的超集）。
2. **`0x4E795` 的返回值是"最后一个掩码字节"**（跳过时=0），不是"最后一次调色板值"。
   反汇编里 `xor eax,eax` 后 `lodsb` 每像素写 AL，掩码为 0 时 AL=0。首版误当"最后调色板值"，
   对拍精确抓出（缓冲一致、仅返回值差）。

### 23.3 实测判据

```
build\utilcheck.exe     →  PASS: 2200 cases, 0 failures
build\rlecheck.exe      →  PASS: 1900 cases, 0 failures（回归）
build\gfxcheck.exe      →  PASS: 1450 cases, 0 failures（回归）
build\sprite24check.exe →  PASS: 2100 cases, 0 failures（回归）
regress.ps1             →  ALL PASS 8/8
```

> 注：对拍 exe 仍偶发 `reserve failed`（某个系统 DLL 被 ASLR 放进 guest 窗口，§8-48 签名 B），
> 重跑即过；`/BASE:0x60000000`（§22.4）解决的是"exe 自己"那一种，进程级重试仍是通用兜底。

### 23.4 下轮入口

1. `0x4E390..0x4E751` 连通性/BFS 簇（`byte_60068/69` 网格、四方向递归、写回 `dword_60073`）——
   **优先归档全局数据布局**再转译，疑似战场移动范围/寻路。
2. 表访问器 `0x4E7DD..0x4E8BC`（`&unk_XXXX + 步长*i`，无逻辑，可在需要时批量转成 `base+stride*i`）。
3. ★★★ `sub_111BA`（资源加载，内部走 Watcom CRT 文件服务）、`sub_15F84`（文本/脚本渲染，usercall）。

---

## 24. 第 24 轮：地形代价洪泛 / 寻路簇转译（0x4E390..0x4E751）（2026-10-05）

第 22 轮地形图里价值最高的一块：`0x4E390..0x4E751` 是**两个入口 + 七个内部函数**，
实现"单位用剩余移动点在带地形代价的格子上做 DFS 洪泛（可达范围）+ 第二遍沿路径回溯并
提交最优路线"——即**移动范围与寻路**。两条入口都是 cdecl（`pusha`/`popa` 全保存），
内部函数走 usercall 寄存器、用 `unk_60079` 当显式回溯栈。

### 24.1 数据结构（从指令逐条推出）

- **地图** `map`：`[+0]=W(u8) [+2]=H(u8)`，`[+4]` 起是 `W*H` 个 **4 字节格子**；
  原版格子指针用 `map + 7 + 4*(y*W+x)`（指向格子**最后一个字节**），所以：
  `cell[-3]`=byte0（地形 id 低字节）、`cell[-2]`=byte1（bit0-1 地形高位→随后成了转向计数 bits2-7）、
  `cell[-1]`=byte2（标志：`0x40`=目标、`0x80`=剩余清零）、`cell[0]`=byte3（剩余移动点）。
- **地形代价**：`t = ((cell[-2]&3)<<8) | cell[-3]`；`cost = cost_row[ cost_table[4*t+1] ]`。
  `cost_table`（原 dword_60060）是地形 id → 代价行下标的映射，`cost_row`（入口 arg0/esi）
  是本单位每类地形的代价。
- **邻居顺序**：右(+4)、左(-4)、下(+4W)、上(-4W)；方向码 右=3/左=1/下=0/上=2。
- 第二遍记录 8 字节 `[dx:2][cl:1][ch:1][cell:4]`；`byte_60077`=深度、`byte_60078`=当前最优路径长、
  `dword_60073`=路径输出缓冲。`0x4E71F` 数当前栈里的**转向次数**（×4），`0x4E751` 在到达目标且
  深度更短时把方向码拷进输出，`0x4E703` 是 mode2 的目标标记。
- 只有"剩余点严格变优"（第二遍允许等点但转向更少，mode1）才继续深入 ⇒ 深度受剩余移动点约束。

### 24.2 转译：`src/game/path.c`

| 转译名 | 原地址 | 角色 |
|---|---|---|
| `path_mark` | `0x4E390` | 入口：一遍洪泛标记可达范围 |
| `path_find` | `0x4E4F6` | 入口：寻路 + 提交最优路线（返回最优长度） |
| `rec1` / `check1` | `0x4E42C` / `0x4E4BE` | 第一遍递归 / 可达性与剩余点更新 |
| `rec2` / `check2` | `0x4E5CC` / `0x4E680` | 第二遍递归 / 带转向择优的检查 |
| `mark_target` / `count_dirs` / `commit_path` | `0x4E703` / `0x4E71F` / `0x4E751` | 目标标记 / 转向计数 / 路径提交 |

原版的 usercall 寄存器参数与显式回溯栈在 C 版换成普通参数与类型化栈（只有 map、输出缓冲、
`byte_60078` 对外可见，观测等价）。**逐条指令对齐**，未引入任何"猜"的高层语义。

### 24.3 对拍（`src/pathcheck.c` + `build.ps1 -Target pathcheck`）

随机小地图（W/H 4..7、格子字节随机、地形代价 1..5、剩余点 1..12 保证深度小）、随机起点/目标/mode。
两条入口都跑原机器码与 C 版，比较 **map 全量 + 输出缓冲 + 返回的最优长度 + 深度全局**。

```
build\pathcheck.exe  →  PASS: 1000 cases, 0 failures（首轮即通过）
```

### 24.4 实测判据

```
pathcheck 1000/0   utilcheck 2200/0   sprite24check 2100/0   gfxcheck 1450/0   rlecheck 1900/0
regress.ps1        ALL PASS 8/8
```

### 24.5 下轮入口

1. 表访问器 `0x4E7DD..0x4E8BC`（`&unk_XXXX + 步长*i`，无逻辑）；计时器/调色板动画
   `0x4E310/0x4E31C`、清键盘 `0x4E381`（需平台层）。
2. ★★★ `sub_111BA`（资源加载，内部走 Watcom CRT 文件服务，独立对拍需先接宿主 `dos.c`）、
   `sub_15F84`（文本/脚本渲染，usercall 多寄存器）。
3. 宿主侧：可开始把已转译的纯模块（rle/gfx/sprite24/util）逐步**接入宿主**（替换原机器码），
   每接一个跑 `regress.ps1` + 帧对拍。
