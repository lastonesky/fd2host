# fd2host — 炎龙骑士团 2（FD2.EXE）原生加载器

**fd2host** 把 `FD2.EXE`（DOS/4GW 32 位保护模式游戏，炎龙骑士团 2）**原生**跑在 Windows 上：
**不模拟 DOS、不模拟实模式、不用 DOSBox**。

做法的核心是一个 LE 加载器：它把 `FD2.EXE` 的 LE 镜像按原始地址布局映射进一个 **32 位 Win32
进程**，保留游戏自己的 x86 机器码原样执行；游戏碰到的平台接口（DOS/BIOS 中断、端口 I/O、
VGA 帧缓冲、键盘、AIL 音频）由宿主用现代实现接管，画面通过 **sokol_gfx**（Windows 上 D3D11）
呈现。游戏的静态数据文件（`*.DAT` 等）照原样从游戏目录读取。

> 本仓库只包含**加载器 + 平台层 + sokol 呈现**，不含任何对 `FD2.EXE` 的逆向/源码转译内容。

## 目录

```
fd2host/
├── README.md         ← 本文件
├── build.ps1         构建脚本（MSVC，32 位目标）
├── aux_build.bat     在已初始化开发者环境的 cmd 里调用 build.ps1
├── Makefile.linux    POSIX 构建（自检 + 64 位链接验证 + host32 真机运行）
├── regress.ps1       一键回归：沙箱里跑到游戏写出 FD2.TMP，8 项断言
├── src/
│   ├── le.c/.h       LE 加载器：对象/页/fixup 重定位、地址空间预留
│   ├── dos.c/.h      平台层：VEH、int/端口接管、DOS/DPMI/BIOS 服务、低内存镜像
│   ├── platform_*.c   OS 缝（线程/文件/时间）
│   ├── host.c/.h      内核：参数、LE/DOS/AIL 启动、游戏线程、调色板→BGRA、抓帧、watchdog
│   ├── entry.c        进程入口 fd2_entry（在 CRT 之前预留地址空间）
│   ├── render.h+render_sokol.c   呈现后端接口 + sokol_gfx 实现
│   ├── main_sokol.c   入口层：窗口/渲染循环、键盘→BIOS 扫描码
│   ├── sokol_impl.c   sokol 的编译单元
│   ├── keylog.c      按键录制/回放（--keylog / --keyplay）
│   ├── keys.c/.h      便携键表
│   ├── winshot.c      窗口截图辅助
│   ├── ail.c+.h        Miles AIL 替换层（游戏用 16 位 AIL，必须整体替换）
│   ├── xmidi.c/.h      XMIDI 解析
│   ├── synth.c/.h      软件合成器
│   ├── dls.c/.h        gm.dls 音色加载
│   ├── audio.h + audio_sokol.c   音乐+音效共用一个软件混音器/设备
│   ├── letest.c       加载器自检
│   ├── platprobe.c    平台自检（预留 guest 窗口并触碰全部对象范围）
│   ├── doscheck.c     DOS 层自检（INT 21h 文件服务、低内存、tick 线程、真 int 0x21）
│   ├── faultprobe*.c  i386/x86-64 故障模型探针（POSIX）
│   └── keyscheck.c    便携键表 vs MapVirtualKeyA
├── vendor/sokol/     sokol 头文件（app/gfx/audio/glue/log/time）
└── build/            输出：fd2host.exe、host.log、*.bmp（不入 git）
```

## 构建

Windows（MSVC，32 位目标）：

```powershell
# 构建宿主（默认即包含加载器 + sokol 呈现）
pwsh -File E:\FD2\port\build.ps1 -Target fd2host

# 需要 vcvars32.bat 的沙箱里也可用：
cmd //c E:\FD2\port\aux_build.bat fd2host
```

Linux（WSL/Debian）：

```bash
make -f Makefile.linux          # letest / platprobe / doscheck / faultprobe + 64 位链接验证
make -f Makefile.linux host32   # 真正能跑的宿主（-m32，默认用 GL 后端）
```

## 运行

游戏按**裸文件名**读资源，所以**工作目录必须是游戏目录**。两个路径参数都不给时，
默认值就取**宿主 EXE 自身所在的目录**（不是任何写死的盘符）：把 `fd2host.exe` 丢进
游戏目录直接运行即可；宿主放在别处时，`--gamedir` / `--exe` 只要给一个，另一个会自动推出。

```powershell
# 默认：游戏目录 = 宿主 EXE 所在目录，加载其中的 FD2.EXE
E:\FD2\fd2host.exe                                  # 直接运行

# 宿主放在别处时，只给一个就够：
E:\tools\fd2host.exe --gamedir=E:\FD2               # 游戏目录 E:\FD2 -> 加载 E:\FD2\FD2.EXE
E:\tools\fd2host.exe --exe=E:\FD2\FD2.EXE           # 游戏目录 = 该 EXE 所在目录

# 抓一帧画面（--screenshot 必须是绝对路径，且配合“抓完即退”）
E:\FD2\fd2host.exe --exit-after=30 --screenshot=E:\FD2\frame.bmp --shot-frame=700
```

日志默认写在**宿主 EXE 旁边**的 `host.log`。⚠ 跑完先看 `host: working directory = …`：
`--gamedir`/`--exe` 都没给时它就是宿主 EXE 所在目录，不是游戏目录就说明参数写错了。

### 命令行参数

所有参数都支持 `--opt value` 与 `--opt=value` 两种写法。

| 参数 | 说明 |
|---|---|
| `--gamedir <dir>` | 游戏目录（工作目录），默认 = 宿主 EXE 所在目录 |
| `--exe <path>` | 要加载的 EXE，默认 `<gamedir>/FD2.EXE`；只给 `--exe` 时反向推出游戏目录 |
| `--exit-after <秒>` | 运行上限，到点干净退出 |
| `--exit-when-file=<路径>:<字节数>` | 某文件写满后（+autokey 跑完）提前干净退出 |
| `--log <路径>` | 日志文件，默认 `build/host.log` |
| `--cmdtail=<string>` | PSP:0x80 命令行尾巴（游戏 spawn/exec 子进程用） |
| `--headless` | 不建窗口（仍运行） |
| `--trace=<n>` | VEH 单步跟踪 n 条指令 |
| `--screenshot=<bmp>` | 抓一帧到 32bpp BMP（**绝对路径**） |
| `--wshot=<bmp>` | 抓窗口内容 |
| `--shot-frame=<n>` | 按帧号触发截图 |
| `--shot-time=<ms>` | 按墙钟触发截图 |
| `--shot-tick=<n>` | 按游戏 BIOS tick 触发截图（跨后端对拍用） |
| `--autokey=<延时ms:VK[,VK...];...>` | 无人值守按键（例 `5000:SPACE;2500:RETURN`） |
| `--keylog=<文件>` | 录制按键（每键一行，收尾打印可回放的 schedule） |
| `--keyplay=<文件>` | 回放 `--keylog` 录制（绝对时间） |
| `--no-user-input` | 忽略真实键盘（自动化回归用） |
| `--volume <0-100>` | 音量，默认 100（自动跑显式用 10） |
| `--ail <none|fd2>` | AIL 替换层开关 |
| `--audio-rate=<hz>` | 音频采样率 |
| `--audio-dump=<wav>` | 把混音器输出录成 WAV |
| `--midi-dump=<wav>` | 离线核对音乐 |
| `--midi-test` | 音乐自检 |
| `--midi-rate=<hz>` / `--midi-backend=<...>` | MIDI 参数 |
| `--gm-bank=<路径>` | 音色库（默认用系统 `gm.dls`） |

## 自检

```powershell
# 加载正确性：把 LE 镜像与参考镜像逐字节对比（无参考镜像时打印对象哈希）
& E:\FD2\port\build\letest.exe

# DOS 层：INT 21h 文件服务 / 低内存 / tick / 真 int 0x21
& E:\FD2\port\build\doscheck.exe

# 平台：预留 guest 窗口并触碰全部对象范围
& E:\FD2\port\build\platprobe.exe

# 键表
& E:\FD2\port\build\keyscheck.exe

# 一键回归（重建沙箱、删 FD2.TMP、autokey 走 continue、8 项断言）
pwsh -File E:\FD2\port\regress.ps1
```

Linux 侧同一套自检：`make -f Makefile.linux && ./build/letest-linux <FD2.EXE> <参考目录> && ./build/doscheck-linux`。
`letest` 会打印三个对象的 `fnv1a`；两个平台哈希相同即证明加载器一致；有参考镜像时应为
`reference check OK, exact match`。

## 几个必须知道的点

- **地址空间布局不可随意改**：游戏对象占 `0x10000..0x6FFFF`、低内存镜像 `0x70000..0x7FFFF`、
  VGA `0xA0000`；低 64 KiB 不可映射。
- **宿主映像必须小且保留 ASLR**（`/DYNAMICBASE` + `/BASE:0x60000000`）：大静态数组或关掉 ASLR
  都会把游戏地址空间挤掉（关 ASLR 时 Windows 会预留低端兼容窗口，正好是游戏对象要用的地方）。
- **地址空间预留必须在 CRT 之前**：自定义入口 `fd2_entry`（`/ENTRY:fd2_entry`），在 `main()` 里做已太晚。
- **不许按字节扫描改写游戏代码**：int 接管走 VEH，读 `[EIP]==0xCD` 直接分派，保持游戏镜像逐字节一致。
- **INT 21h/31h 的返回值语义错一个就跑飞**；改 `dos.c` 前先看 `src/dos.h` 与注释里的语义表。
- **工作目录必须是游戏目录**（游戏按裸文件名开资源）。
- **AIL 的 `*.DIG`/`*.MDI` 是 16 位实模式代码，任何路线下都整体替换**，不要尝试执行。
- 调色板是 6 位 DAC，转 32bpp 时要做 `(v<<2)|(v>>4)`；`BI_RGB` 内存序是 **BGRA**。
