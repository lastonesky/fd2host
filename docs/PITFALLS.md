# 踩坑清单与历史卡点
**动手前先通读 §8。** 每一条都写清了根因与实证判据，能省几天。
另附 §6「历史卡点」（早期两个已修复的崩溃，保留作诊断参考）。
对应旧 `PROGRESS.md` §8、§6。条目编号 `§8-N` 沿用旧编号，外部引用无需改号。
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

52. **在"提交时"烘焙增益的流式播放，预排队缓冲会吃到过期音量**（第 35 轮）：
    `synth_play` 原来在 `AIL_start_sequence` **内部**就填满 4 × 2048 样本（371 ms）并
    `waveOutWrite`，而游戏是在 `start` 返回后微秒级才 `set(seq,0,0)` + `set(seq,127,2000)`
    ⇒ 新曲开头 371 ms 是**上一首留下的电平**（满音量爆一下再掉进淡入），原版 AIL 由定时器
    驱动、`set(0,0)` 抢在第一个样本进设备之前，没有这个窗口。
    - **判据**：`--midi-dump` 离线渲染看第 1 个音的时刻与前 371 ms 峰值
      （标题曲：0.2 ms / 峰值 17709 ⇒ 不是静音）；运行日志
      `synth: stream released after 0 ms -> … gain 0.000` = 首队列按游戏要求的电平填。
    - **修法**：起播闸门 —— `synth_play` 只 `PrepareHeader`，`stream_thread` 停在
      `g_arm`，第一条 `synth_set_sequence_volume` 放行（200 ms 超时兜底）。
    - **连带坑**：闸门期间**从未 `waveOutWrite` 过**的 header 永远不会置 `WHDR_DONE`，
      `synth_stop()` 里"等 DONE（100 × 10 ms）"会每片空转 1 s、停曲卡 4 s ⇒ 只对
      `g_queued[i]` 的片等。

53. **同 BIOS tick ≠ 同状态：取样点落在过渡动画上，连同后端自比都能差 60%**（第 36 轮）：
    `--shot-tick=290`（片头转场中）连跑两次**同一个** sokol/GDI 后端，`framediff` 得到
    **36949/64000（57.7%）**、**38613/64000（60.3%）**，两张图一张是黑场转场帧、一张是
    已入戏的场景帧。tick 只钉住宿主那条 18.2 Hz 的 `bios_tick_thread`；那一段游戏状态还取决于
    "启动到进该场景花了多久"（加载/调度抖动），于是同一 tick 落在时间轴的不同位置。
    - **判据顺序不能反**：先做**同后端基线自比**（同参数连跑两次 → 必须 `0 px`），
      基线不为 0 就说明**取样点选错了**，此时跨后端差多少都没有意义。
    - **正确做法**：取样点选**静止画面**（标准 autokey 走完、停在静态等键态）。
      实测 `--shot-tick=600`：同后端基线 **0 px**，GDI vs sokol **31 px（0.0484%）**，
      差异全在一块 14×4 的动画元素相位上（见 `BACKEND.md` §13.10）。

54. **`WHDR_DONE` 只对"进过队列的缓冲"有意义；判据也不能只做到"填了缓冲"**（第 35/38 轮）：
    §8-52 的起播闸门把 `waveOutWrite` 推迟到游戏报音量之后，`stream_thread` 却仍用
    `if (!(dwFlags & WHDR_DONE)) continue;` 当"可重填"判据 —— **`waveOutPrepareHeader`
    只置 `WHDR_PREPARED`(0x2)，从未 `Write` 的缓冲永远等不到 `DONE`(0x1)** ⇒ 闸门放行后
    依然全部 `continue`，**4 个缓冲一个都没进 waveOut：音乐整个没了，音效照常**（SFX 走
    另一条设备路径，所以这个 bug 只哑音乐）。
    - **同一个坑当时已写在 §8-52**，但只用在 `synth_stop` 的等待上，没回头检查**同一判据
      在播放循环里的另一半** —— 改了流程就要把该流程上**所有**用到旧前提的地方过一遍。
    - **修法**：`if (g_queued[i] && !(dwFlags & WHDR_DONE)) continue;` —— 只有驱动拥有的
      缓冲才等 `DONE`，没进过队列的本来就是我们的。
    - **判据教训（更值钱的一半）**：上一轮的证据 `stream released … gain 0.000` 与
      `fading 0 -> 127 …` 只证明"**按什么电平填了缓冲**"，**没证明缓冲进过设备**，
      于是照样发布了一个**没有音乐**的版本。音频类判据必须到设备层：
      `synth: stream alive - N slices, pos P (type T), gain G, queued peak X/32767`
      —— N/pos 单调前进 = 设备在消费，`peak > 0` = 内容非静音。

55. **过渡画面抓图：同配方同 tick 跨运行也会落在不同画面，必须重试 + 用像素判据挑帧**
    （第 40 轮，是 §8-53 的执行细则）：autokey 按**墙钟**发键，游戏状态推进还受加载/调度
    抖动影响，于是同一配方、同一 `--shot-tick` 的两次运行可能一个在台词打字中、另一个在
    章节选择菜单（实测 tick323 = 菜单、tick326/329 = 打字中、tick345 = 菜单）。
    - **判据顺序**：先在目标窗口连取几个 tick，拿"目标态参考帧"算逐像素差挑出正确帧
      （本轮 框区差 455→327→325→**0** 就是进度曲线）；**静止段反而可靠** —— 打完之后
      tick380..590 连续 10 次抓图 **0 px 差**。
    - **省时间的抓法**：`--screenshot=<bmp>` + `--exit-when-file=<同bmp>:256054`，抓完 2 s 即退；
      只写 `--exit-after=60` 会在静止画面上白等几十秒（单轮 60 s → **22 s**）。
    - **BMP→PNG 别用 `Save(路径)`**：`[System.Drawing.Image]::FromFile(b).Save(p)` 存的是**原图
      格式**（产物头 `BM`），文件名 `.png` 骗过文件名骗不过看图器 = 花屏。用
      `python tools\bmp2png.py in.bmp out.png`，或 `.Save(p, [System.Drawing.Imaging.ImageFormat]::Png)`。
    配方与完整判据见 `docs/rounds/10-typewriter-recipe.md` §40。
