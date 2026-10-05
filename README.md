# FD2(炎龙骑士团2 Flame Dragon 2 黄金城之谜) → Windows 原生移植工程（路线 C：二进制宿主 + 逐步源码化）

目标：让 `E:\FD2\FD2.EXE`（DOS/4GW 32 位保护模式游戏，Borland/Watcom + Miles AIL）
**在 Windows 上原生运行**——不模拟 DOS、不模拟实模式、不使用 DOSBox。
图形改为现代 Windows 绘制，声音改为现代音频后端，游戏逻辑暂时保持原始 x86 机器码。

**当前里程碑：POC 达成** —— 游戏原生运行、持续渲染开场动画（30 秒 960 帧无崩溃），画面颜色
正确，**并且有声音**（数字音效 + XMIDI 音乐）。详见下文"当前状态与下一步"。

## 目录

```
port/
├── build.ps1          构建脚本（vcvars32 + cl，32 位目标）
├── regress.ps1        一键回归：重建沙箱（故意删 FD2.TMP）→ autokey 走 continue → 8 项断言
├── src/
│   ├── le.h / le.c    LE(Linear Executable) 加载器：解析对象、页、fixup 重定位
│   ├── dos.h / dos.c  平台层：VEH 捕获 int/特权指令 + DOS/DPMI/BIOS 服务替换
│   ├── host.c         宿主主程序：地址空间预留、窗口、显示、键盘、主循环
│   ├── repl.h/repl.c  **源码接入层**：把已验证的转译函数入口改成 jmp 到 C 实现（默认开，`--replace=` 可关）
│   ├── letest.c       加载器自检（与 Ghidra 导出的重定位镜像逐字节对比）
│   ├── game/          **源码转译产物**（从逆向还原的 C 源码，带原地址注释）
│   │   ├── rle.h/rle.c   RLE 解码器（原 0x4E98D/0x4E8D3，PROGRESS §19）
│   │   ├── gfx.h/gfx.c   图形 blit 工具族（save/restore rect、block/透明 blit、16×16 字形、scanline 重排，PROGRESS §21）
│   │   └── sprite24.h/.c 24×24 精灵 RLE 族（7 种颜色模式，PROGRESS §22）
│   │   └── util.h/util.c 字节/调色板工具（查表翻译/校验和/反混淆/掩码重着色，PROGRESS §23）
│   │   └── path.h/path.c 地形代价洪泛与寻路（移动范围/最优路线，PROGRESS §24）
│   │   └── res.h/res.c   LMI 容器资源加载（原 0x111BA，PROGRESS §25）
│   ├── rlecheck.c     转译对拍测试：随机 RLE 流 × 原机器码 vs 转译 C，逐字节比对
│   ├── gfxcheck.c     转译对拍测试：图形 blit 工具族 × 原机器码 vs 转译 C，逐字节比对
│   ├── sprite24check.c 转译对拍测试：24×24 精灵 RLE 族 × 原机器码 vs 转译 C，逐字节比对
│   ├── utilcheck.c    转译对拍测试：字节/调色板工具 × 原机器码 vs 转译 C，逐字节比对
│   ├── pathcheck.c    转译对拍测试：地形代价洪泛/寻路 × 原机器码 vs 转译 C，逐字节比对
│   ├── rescheck.c     转译对拍测试：资源加载器（把 CRT 文件/内存调用重定向到 libc 后调原机器码）
│   ├── ail.c          AIL 替换层：16 个 AIL_* 入口 → 宿主实现（数字音效走 WinMM waveOut）
│   ├── xmidi.c        XMIDI 解析（FDMUS.DAT 的 XDIR/CAT/FORM XMID）→ 事件列表
│   ├── synth.c        自带软件合成器：事件 → PCM → waveOut（音乐不依赖系统 MIDI）
│   ├── dls.c          DLS Level 1 解析：读 gm.dls 的原版 GM 采样供合成器使用
│   ├── probe*.c       可行性探针（地址空间 / ASLR / 映像大小）
├── re/                逆向工作台产物：RE_MAP.md（测绘与转译地图）、funcmap.csv（1359 函数表）、
│                      int21_ah_used.txt（INT21 AH 静态覆盖）、int_sites_all.txt（CD xx 裸扫）等
├── PROGRESS.md        进度存档 / 交接文档（含踩坑清单与调试手册）
└── build/             输出：fd2host.exe、host.log、object*.bin（Ghidra 参考镜像）、frame*.png（画面证据）
```

## 构建与运行

```powershell
pwsh -File port\build.ps1 -Target fd2host     # 生成 port\build\fd2host.exe
Start-Process port\build\fd2host.exe -ArgumentList '--exit-after=30' -WorkingDirectory 'E:\FD2'

# 源码转译对拍（第 19 轮起）：转译 C vs 原始机器码逐字节比对，全过才算转译正确
pwsh -File port\build.ps1 -Target rlecheck ; & port\build\rlecheck.exe   # 1900 例
pwsh -File port\build.ps1 -Target gfxcheck ; & port\build\gfxcheck.exe   # 1450 例
pwsh -File port\build.ps1 -Target sprite24check ; & port\build\sprite24check.exe  # 2100 例
pwsh -File port\build.ps1 -Target utilcheck ; & port\build\utilcheck.exe      # 2200 例
pwsh -File port\build.ps1 -Target pathcheck ; & port\build\pathcheck.exe      # 1000 例
pwsh -File port\build.ps1 -Target rescheck ; & port\build\rescheck.exe        # 160 例

# 跑别的 DOS/4GW(LE) 游戏（通用化见 PROGRESS.md §14，先做静态体检）
python port\re\preflight.py "E:\Games\FDCollection\Game\FDPS\FDPS.EXE"
Start-Process port\build\fd2host.exe -ArgumentList `
  '--exe=E:\Games\FDCollection\Game\FDPS\FDPS.EXE', '--gamedir=E:\Games\FDCollection\Game\FDPS', '--exit-after=30'
```

日志写入 `port/build/host.log`（宿主是 WINDOWS 子系统，不弹控制台窗口）。
可用参数：`--gamedir <目录>`、`--exe <路径>`、`--exit-after <秒>`、`--trace=<n>`、`--headless`、
`--replace=<none|all|rle,gfx,sprite24,util,path>`（默认 `all`：把已对拍的转译函数接入游戏；`none` = 原机器码，用于 A/B）
**所有带值的参数都同时支持 `--opt value` 与 `--opt=value` 两种写法**（`host_init()` 统一归一化，
另一种写法不再静默回退到默认值，见 `PROGRESS.md` §8-32/§8-33）、
`--exit-when-file=<路径>:<字节数>`（文件写满且 autokey 跑完 → 提前干净退出 + 2 s 缓冲，
退出前抓最后一帧；与 `--exit-after` 上限配合，回归单次 **~15 s**，见 `PROGRESS.md` §20）、
`--screenshot=<file.bmp>`、`--shot-frame=<n>`（在第 n 帧导出实际送显的 RGB 缓冲，默认 300）；
`--cmdtail=<尾巴>`（写进 PSP:0x80 的命令行，`INT 21h AH=4B` 拉起子进程时自动传递）、
`--log=<路径>`（换日志文件；子进程各用各的 `host.<pid>.log`，否则会截掉父日志，见 `PROGRESS.md` §8-47）；
音频：`--ail-dump=<目录>`（导出音效样本与 XMIDI 原始数据）、`--ail-rate=<Hz>`、`--ail-bits=<8|16>`、
`--ail-stereo`、`--midi-rate=<ticks/s>`（默认 0 = 按序列 tempo 换算）、
`--midi-backend=<synth|winmidi>`（默认 `synth` = 自带合成器）、`--midi-test`（播测试音）、
`--gm-bank=<path>`（默认用 Windows 的 `gm.dls` 提供原版 GM 音色）。详见 `PROGRESS.md` §11。

```powershell
# 抓第 700 帧画面（不依赖窗口/桌面，便于核对调色板与通道顺序）
Start-Process port\build\fd2host.exe -ArgumentList '--exit-after=30','--screenshot=E:\FD2\port\build\frame.bmp','--shot-frame=700' -WorkingDirectory 'E:\FD2'
# 日志会打印 "host: frame 700 dumped" 与 "watchdog fired after 30 s (960 frames drawn)"
```

## 已验证的关键事实（对抗过 Ghidra 镜像逐字节校验）

| 项目 | 结论 |
|---|---|
| 容器 | `MZ` + DOS/4GW stub + LE 头 @0x27ACC；3 个对象：0x10000/0x50000/0x60000 |
| 对象数据 | 页对齐、紧凑存放于文件 0x36014 起（EOF 减去各对象页跨度和） |
| 入口 | obj0 + 0x2CCB4 = **0x3CCB4** |
| fixup 记录 | 每页一块，记录 `[07][size][src:2][obj:1][tgt:2+size>>4]`；`[00]` 为 1 字节填充 |
| 跨页记录 | 源操作数跨页（src≈0xFFFD..0xFFFF）的记录必须**跳过**，否则破坏下一页开头（会直接崩在 0x3E000） |
| 重定位结果 | obj1/obj2 与 Ghidra 镜像**逐字节一致**；obj0 仅余 11 字节待确认（同意在页边界） |
| 宿主映像 | 必须 < ~1MB 且保持 ASLR：静态 8MB 数组会让 ASLR 把映像塞进 0x10000，游戏地址空间就抢不到了 |
| 地址空间 | 32 位进程可精确映射 0x10000..0x6FFFF（RWX）与 0xA0000（RW）；**低 64KB 不可映射** |
| DOS/4GW 私有选择器 | 启动代码把 0x24 之类当段选择器加载；本进程 GDT 槽 4 是代码段 → 必须替换为宿主平坦选择器 |
| PSP/环境 | 启动代码用 `ES:[0x2C]` 之类访问 PSP；把 IVT 0x2C 的"实模式段"设为 0x7000 后，`段<<4` 正好落在低内存镜像里 |
| INT 21h AH=0xFF | 返回 AL=0 会被当成"未知扩展器"（走进空环境指针）；返回非 0 才会读 PSP:0x2C |
| INT 21h AH=0x48 | 返回**线性地址**（不是段值），调用方直接解引用；且必须读**完整 EBX**（Watcom `_ExpandDGROUP` 传 `0x10000`，只取 BX 会截断成 0）；EBX=0 要按 DOS 返回失败 |
| INT 21h AH=0x42 | lseek：**入参 CX:DX、出参 DX:AX**。把 CX 当 64 位偏移高半会让文件指针跳到 ~171 GB 且**不报错**，后续 read 返回 0 字节 |
| INT 31h AX=0x0501 | BX:CX 传入字节数、返回线性地址；以前返回假值会让游戏拿到野指针 |
| INT 16h | **菜单导航依赖它**（`AH=0x10` 读扩展键，游戏按返回的 `AH` 扫描码判断：0x4B 左/0x4D 右）。只做 BDA 轮询时，片头能跳过但菜单方向键全无反应 |
| GM 音色库 | 打击乐在 gm.dls 里靠 `ulBank` 的 **bit31** 标识（不是 bank 号）；MIDI 的 `(CC0<<7)\|CC32` 与 DLS 的 `(msb<<8)\|lsb` 需要转换；CC7/CC11 决定各音轨电平 |
| AIL 入口 | 必须把**全部 52 个**入口（51 个导出 + `AIL_install_timbre` 0x3B80F）都 patch 掉；只封"游戏直接调用的 16 个"会让 continue 路径执行到原始 AIL 代码并跳飞 |
| 调色板 | VGA DAC 只有 **6 位/通道**：`dos_palette` 统一存 8 位值（捕获时 `(v<<2)\|(v>>4)`）；32bpp `BI_RGB` 内存序是 **BGRA** |
| 1 MiB 内实模式区 | `0x80000..0x9FFFF` 与 `0xC0000..0xFFFFF` 预先 commit：真机上 0xC0000+ 是 ROM（写入丢弃），游戏会**合法地**越过 VGA 窗口写到那里 |
| AIL 驱动 | `*.DIG`/`*.MDI` 是 16 位实模式代码，AIL 会跳进去执行 ⇒ 必须整体替换；POC 阶段报"文件不存在"让其以无设备启动 |
| 文件创建/截断 | `fopen("wb")` 对尚不存在的文件会走 `INT 21h AH=3C`(CREAT)；`O_TRUNC` 靠 `AH=40` **写 0 字节**实现，而 DOS 的"写 0 字节 = 在当前位置截断"在 Windows 上是空操作。两条缺任一条都会崩或静默损坏存档 |
| 鼠标 | 游戏**不用鼠标**：obj0 无 `int 33h` 调用点（唯一的 `CD 33` 在 DOS/4GW 桩表里）、`int386` 只用 0x10/0x16/0x31、运行期 `int 33` 调用数 0 |

## 当前执行进度（host.log 实证）

```
LE 加载 + 7937 条 fixup 应用
→ VEH 安装，112 个 int 站点改写为 int3 并接管
→ 进入游戏入口 0x3CCB4
→ DOS/4GW 环境探测 / PSP / 环境解析        ✅
→ C 运行库初始化（sbrk、AH=2C 计时）        ✅
→ AIL 探测驱动：SBPRO2.MDI / SB16.DIG      ✅ 已拦截
→ 打开 DIG.INI / MDI.INI / FDOTHER.DAT / FDTXT.DAT  ✅
→ INT 10h AH=0 设置视频模式 0x13（320x200x256）     ✅
→ 调色板端口 I/O（0x3C8/0x3C9）             ✅
→ 按 DAT 偏移表加载资源、RLE 解压到帧缓冲   ✅（修复 AH=42 lseek / AH=48 EBX 之后）
→ 开场动画持续渲染                          ✅ 30 s / 960 帧 / port ops 80 万+ / 无崩溃
→ 画面颜色正确                              ✅ 帧 250 与帧 700 画面不同 = 动画在推进
→ 声音：16 个 AIL 入口替换为宿主实现         ✅ 音效 waveOut（8 位单声道 11025 Hz）
→ 声音：XMIDI → 自带合成器 → waveOut        ✅ 2838 事件 / 112 BPM / 317.5 s / polyphony 31
→ 声音：原版 GM 音色（解析 gm.dls）         ✅ 235 乐器 / 495 采样加载，2781/2781 音符命中采样
→ 键盘：INT 16h（菜单导航）                 ✅ 方向键可用、带导航音效（片头跳过走的是 BDA 轮询）
→ 平台层文件服务：AH=3C 创建 / AH=41 删除 / AH=40 写 0 字节截断  ✅ fresh install 不再崩，regress.ps1 8/8 PASS
→ 游戏退出路径（INT10 mode 3 → AH=4Ch → ail shutdown）           ✅ 菜单主动退出实测（PROGRESS §12.4）
→ 第 1 步接口抽取：render.h / host.h / main_win32.c + `-Render gdi|sokol`  ✅ GDI 成为第一个后端，regress 8/8
→ sokol 选型实测：0 DLL、exe +146 KB、Win 上直接 D3D11            ✅ 头文件已 vendor（pin 2e75443）
→ 第 14 轮通用化：`--exe` 可跑任意 LE 游戏（10 处 FD2 专属依赖补齐，见 PROGRESS §14）  ✅ FD2 回归 8/8
→ 炎龙外传 FDPS 首跑：LE/fixup/低内存自动挪位/启动链/设 13h 模式/调色板/首帧  ✅ 120 s 无崩溃
→ 第 15 轮：FDPS 专属 90 条 AIL 入口表 + 宿主定时器线程  ✅ 动画时钟精确 25 Hz，标题不再卡首帧
   └ 实测 `ail: timer fire #100 at +4000 ms`；帧 700 = 82 色非黑；FD2 回归 8/8 PASS（PROGRESS §15）
→ 第 16 轮：`INT 21h AH=4B`(EXEC)  ✅ 真开子进程拉起 FD.EXE，尾巴 19 字节逐字节正确（PROGRESS §16）
   └ 新坑：DOS 尾巴是 `[len][chars][0x0D]` 不是 C 串；`rep scasb` 扫低内存需宿主整条模拟
   └ 当前卡点（已过）：`FD1.Aud`/`FD1.Vid` 全集都不存在 ⇒ FD.EXE `exit(8)`（空文件也不行）
→ 第 18 轮：游戏自挂 **INT 9** 投递 + 修 `type 0x02` fixup 写宽度  ✅ 标题菜单 → START NEW GAME → 进游戏场景
   └ 判据：`build/fdps_menu2.png`（游戏场景，97 色）vs `fdps_static.png`（标题菜单，82 色）；FD2 回归 8/8（PROGRESS §18）
   └ 当前卡点：场景读完 `FACE.CEL` 后跳到 `EIP=0x1FFFC`（解引用 `0x43B4` < 64 KiB）
→ 第 19 轮：**源码转译开工** —— RLE 模块 0x4E98D/0x4E8D3 → `src/game/rle.c`，机器码对拍 **1900 例逐字节一致**
   └ 同轮按用户决定**停止 FDPS 支持**（PROGRESS §7.8 冻结），全力回到“逆向为高级语言代码”（PROGRESS §19）
→ 第 20 轮：回归提速 —— `--exit-when-file` 完成即退出 + 脚本轮询，**75 s → 15 s**；低地址被加载器占用时自动重试（PROGRESS §20）
→ 第 21 轮：**图形 blit 工具族转译** —— 0x4ECBF/0x4EC7C/0x4ED0B/0x4ED34/0x4ED7A/0x4EEE0 → `src/game/gfx.c`，机器码对拍 **1450 例逐字节一致**；同轮评估官方逆向知识库 `docs/`（发现其 FD2.EXE 是另一 build，见 §21.1）
→ 第 22 轮：**24×24 精灵 RLE 族转译** —— 7 个颜色模式变体 → `src/game/sprite24.c`，对拍 **2100 例逐字节一致**；`docs/` 清理到 9 MB/274 文件（`docs/KEEP.md`）；修复对拍 exe 自己被 ASLR 放进 guest 窗口的问题（加 `/BASE:0x60000000`）
→ 第 23 轮：**字节/调色板工具转译** —— 6 函数 → `src/game/util.c`，对拍 **2200 例逐字节一致**；发现 `0x4DF09` 不守 ABI（改 EBX 不保存）与 `0x4E795` 返回值语义两个坑
→ 第 24 轮：**地形代价洪泛/寻路转译** —— `0x4E390..0x4E751`（2 入口 + 7 内部）→ `src/game/path.c`，对拍 **1000 例逐字节一致**；确认这是单位的**移动范围 + 最优路径**算法（地形代价表 + 四方向 DFS + 转向择优）
→ 第 25 轮：**资源加载器转译 + CRT 重定向对拍术** —— `0x111BA` → `src/game/res.c`，对拍 **160 例逐字节一致**；新方法：把 Watcom CRT 的文件/内存入口换成宿主 libc 后再调原机器码，**解锁依赖文件/内存的函数测试**（下一步 `sub_15F84` 可用）
→ 第 26 轮：**转译代码接入宿主** —— 新增 `src/repl.c`，把 23 个已对拍函数入口 jmp 到 C 实现，游戏**真的在跑转译代码**；`regress.ps1 -Replace none/all` 均 8/8，固定帧 150 对拍与基线噪声一致；同轮修复无 `--exit-when-file` 时 `--exit-after` 2 s 早退的 bug
```

## 当前状态与下一步

**POC 目标"窗口中看到游戏画面"已达成**（2026-10-04）。已解决的问题（细节见 `PROGRESS.md` §6 与 §8 第 15–17 条）：

| 现象 | 根因 | 修复 |
|---|---|---|
| RLE 解压写飞，崩在 `0xC0005`（= 段 0xC000×16） | `INT 21h AH=42`(lseek) 把 CX 当成 64 位偏移的高半 → 文件指针跳到 ~171 GB，资源读到 0 字节 | 按 DOS 语义解析 **CX:DX 入参 / DX:AX 出参** |
| `__ExpandDGROUP` 越界写 `0x48FFFF8` | `INT 21h AH=48` 用 `Ebx & 0xFFFF` 截断 `0x10000` → 只给了 16 字节块 | 读**完整 EBX**；EBX=0 按 DOS 返回失败 |
| 画面红蓝互换（黄色显示成青蓝） | 32bpp `BI_RGB` 是 **BGRA** 内存序，代码按 RGB 填充 | `(c[0]<<16) \| (c[1]<<8) \| c[2]` |
| 亮度/饱和度只有约 25% | VGA DAC 是 **6 位/通道**，原始值被当 8 位使用 | 捕获时 `(v<<2) \| (v>>4)` 伸展到 8 位 |
| `fopen("wb")` 打不开新文件 → `fwrite(NULL)` 崩在 0x377B2（读地址 0xC） | 宿主没实现 `INT 21h AH=3C`(CREAT)；且 `AH=40` 写 0 字节不截断 | 补 `AH=3C`/`AH=41` + 显式 `SetFilePointer`+`SetEndOfFile`（PROGRESS §12） |

**下一步（按优先级）**
1. **显示层现代化 = sokol（已定）**：`0xA0000` 的 8bpp 缓冲 + `dos_palette` → **sokol_gfx**
   （320×200 RGBA8 流式纹理 + GPU 缩放，Windows 上自动走 **D3D11**）；GDI 保留为 `--render=gdi` 对拍基准。
   **交付 0 DLL**（实测：exe +146 KB，依赖只有 d3d11/USER32/GDI32/SHELL32/KERNEL32 等系统库）；
   `swap_interval=1` 顺带解掉 32 fps 的定时器限制。
   选型/实测数据/代价清单（手写 shader、sokol_main 入口改造）见 `PROGRESS.md` §13.1；
   SDL2/SDL3 实测对比降为备选记录（§13.2/§13.3）。
2. ~~**AIL 替换层 + 音乐**~~ **已完成**：16 个 `AIL_*` 入口已替换 —— 音效走 WinMM waveOut；
   音乐由 `synth.c` 自带合成器渲染成 PCM 后走同一条 waveOut 通路（**不依赖系统 MIDI**，
   原因见 `PROGRESS.md` §11.1）。可继续打磨：音效循环（`loop_count > 1`）、音量/声像、
   更真实的乐器音色（当前是基频 + 2/3 次谐波的近似音色）。
3. **输入层**：BIOS 键盘缓冲已可写入（BDA 0x41A/0x41C + 0x41E 环形缓冲）；
   鼠标 `INT 33h` **已判定不需要**（游戏不用鼠标，证据见 `PROGRESS.md` §12.3）。
4. **稳定性长跑**：连续运行 5 分钟以上；游戏退出路径已验（`int386(0x10)` mode 3 → `AH=4Ch` → `ail: shutdown`）。
5. **首次保存实测**：`FD2.SAV` 不存在时的创建路径（机制已通，回归只覆盖了 `FD2.TMP`）
   + "存档变小"时的截断对拍。可复现回归：`pwsh -File port\regress.ps1`。
6. **逐步源码化（路线 C 主体）**：按 `re/RE_MAP.md` 的模块顺序把机器码替换为 C 源码，
   最终形成可编译 x86-64 的引擎。**已验证并对拍**：RLE 解码（`rle.c`，1900 例）、图形 blit
   工具族（`gfx.c`，1450 例）、24×24 精灵 RLE 族（`sprite24.c`，2100 例）、字节/调色板工具
   （`util.c`，2200 例）、地形代价洪泛/寻路（`path.c`，1000 例）、资源加载（`res.c`，160 例）；
   **其中 23 个已通过 `src/repl.c` 接入运行中的游戏**（默认 `--replace=all`）；下一批：`sub_15F84`
   文本/脚本渲染、表访问器 `0x4E7DD..0x4E8BC`，以及 CRT 堆/文件层整体替换（`res.c` 随后接入）。
7. **跨平台**：单代码库 + 后端选择（**不用 git 分支**）。已抽出的是 `render.h`/`host.h` +
   入口层 `main_win32.c`（第 1 步完成）；`audio.h` 随**第 3 步**（sokol_audio 替换 waveOut）抽，
   `platform.h`（OS 适配：内存/线程/文件/异常）随**第 4 步** POSIX 一起抽（届时才引入
   `platform_win32.c`/`platform_posix.c`，在那之前 Win32 调用仍留在 `dos.c`/`ail.c` 原处）。
   先出 **Linux x86-64**，ARM 需完成源码化。顺序见 `PROGRESS.md` §13.5/§13.6；sokol 实测见 `§13.1`。
8. ~~**FDPS（炎龙外传）跑起来**~~ **已冻结（2026-10-05 用户决定：不再继续支持 FDPS）** ——
   已达成的成果存档：25 Hz 动画时钟（§15）、`AH=4B` 拉起 FD.EXE（§16）、游戏自挂 INT 9 注入（§18）、
   标题菜单 → 游戏内场景（`build/fdps_menu2.png`）；宿主通用能力（`--exe`、FDPS AIL 表、定时器线程）
   保留在代码里不再主动维护。FDPS 后续卡点（§18.5 `FACE.CEL` 跳飞、`FD1.Vid` 缺失）不再投入。

## 调试手法（可复用）

- `letest.exe`：加载器 vs Ghidra 镜像逐字节对比，是加载正确性的唯一可信判据。
- `rlecheck.exe`：源码转译 vs **原始机器码**逐字节对拍（随机流 1900 例 + 全局副作用），
  是转译正确性的唯一可信判据（方法见 PROGRESS §19.4，可复用于后续每个模块）。
- `gfxcheck.exe`：图形 blit 工具族的同类对拍（1450 例，含哨兵余量与 scratch 全局断言），
  覆盖 save↔restore、block/透明 blit、16×16 字形、scanline 重排（PROGRESS §21.3）。
- `sprite24check.exe`：24×24 精灵 RLE 族 7 变体的对拍（2100 例），验证颜色映射与 type 11
  特殊 token（跳过/重着色/填 0x49）（PROGRESS §22.3）。
- `utilcheck.exe`：字节/调色板 6 函数的对拍（2200 例，含返回值与 `word_6017B` 断言）；
  `0x4DF09` 不守 ABI，测试用 asm 保存/恢复 EBX（PROGRESS §23.2）。
- `pathcheck.exe`：地形代价洪泛/寻路的对拍（1000 例，比较 map 全量 + 输出缓冲 + 最优长度）（PROGRESS §24）。
- `rescheck.exe`：资源加载器的对拍（160 例）。它先把游戏 CRT 的 `fopen/fclose/fseek/fread/malloc/free`
  入口改成 jmp 到宿主 libc，再调**原版 `0x111BA`**——这套"CRT 重定向"术可测试任何依赖
  文件/内存的游戏函数（PROGRESS §25.2）。
- 所有走 `le.c` 的 console 对拍 exe 都链 `/BASE:0x60000000`：否则 exe 自己的映像会被
  ASLR 放进 guest 窗口 `0x10000..0x6FFFF` 导致预留失败（PROGRESS §22.4）。
- `--screenshot=<file.bmp> [--shot-frame=<n>]`：导出**实际送显**的 RGB 缓冲，不依赖窗口/桌面，
  用于核对调色板与通道顺序；BMP→PNG 可用 `[System.Drawing.Image]::FromFile(...).Save(...)`。
- 崩溃转储会打印：EIP 前后 48 字节、`RLE w/h (@0x627B4)`、`[ESI]` 源字节、`[ESP]` 返回地址、
  EBP 帧的前 6 个参数、全部 INT21/INT31 分配块、最后被接管的中断站点。
  `/MAP:fd2host.map` 可把宿主 RVA 反查成符号。
- `--trace=<n>` 打开单步跟踪（VEH 里置 TF），用于跟丢执行流时定位。
- Ghidra 本地 HTTP 桥（`http://127.0.0.1:8089`，`/read_memory`）可**不经上下文**批量导出镜像与内存。
- IDA Pro 9.5 + ida MCP（用法见 `PROGRESS.md` §10）：逆向与源码转译的主工作台，
  测绘结果在 `re/RE_MAP.md`、全量函数表 `re/funcmap.csv`。
- `port/docs/`：`github.com/wicanr2/fd2_re`（同游戏 Go 重制）的 docs 快照（`.gitignore` 已忽略，
  不入库）。**只作语义来源**：它是**另一个 FD2.EXE build**（md5 `b97caf22…`，本项目 `a6e341a8…`），
  地址/常量/指令以其 `E:\FD2\FD2.EXE.i64` 为准（PROGRESS §21.1）。
