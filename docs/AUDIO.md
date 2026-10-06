# 声音：AIL 替换层 / XMIDI / 合成器 / gm.dls
16 个 AIL 入口的替换策略、为什么不用系统 MIDI、XMIDI 与标准 SMF 的差异、
多音分配、gm.dls（DLS Level 1）解析与打击乐的 bit31 标识、快速 loc。
对应旧 `PROGRESS.md` §11。
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

### 11.6 SFX 的"瞬间刺啦一下"（杂音）定位与修复（2026-10-06）

**症状**：音效偶尔伴一声短促的"刺啦"（爆音/click），不是持续噪声。

**已验证的根因**（`src/ail.c` 修复前的代码路径）：

1. **每个音效都拆装一次音频设备**。`svc_play_sfx`（`0x25A96`）开头就是
   `AIL_stop_sample`，于是每次播放都走：
   `host_AIL_stop_sample` → `sample_close_device` → **`waveOutClose`**，
   紧接着 `host_AIL_start_sample` → `sample_play` → **`sample_open_device` → `waveOutOpen`**。
   而且 `AIL_init_sample` 里还**再关一次**（init/addr/loop/start 每个音效都调一遍）。
   ⇒ 一次音效 = 一次设备拆装；实测单局 **47 次 play / 49 次 stop**（20 s 跑），约 1.7 次/秒。
2. **一次 `waveOutWrite` 提交整段 PCM，没有任何淡入淡出**。8-bit 静音电平是 128，若样本首
   字节不是 128，设备启动瞬间就是一个阶跃；播完回到空闲又是第二个阶跃。DOS 声霸卡对这种
   波形很宽容，waveOut 不是。
3. 实测 SFX 参数：**667 字节 @ 11025 Hz 8-bit mono = 60 ms**，非常短，于是"起—停"两次阶跃
   在一声里占的比例很大，格外明显。

**修复（三处，都不改游戏语义）**：

| 改动 | 做法 |
|---|---|
| 设备常驻 | 新增 `sample_stop()`：`waveOutReset` + 等 `WHDR_DONE`（≤200 ms 有界轮询，与 `synth_stop` 同款）+ `waveOutUnprepareHeader`，**保留 `HWAVEOUT` 不关**。`sample_close_device()` 只在格式变更与 `ail_shutdown` 时调用。`AIL_init_sample` 从"关设备"改成 `sample_stop()` |
| 起停斜坡 | 新增 `pcm_apply_ramp()`：在宿主 PCM 副本首尾各 **`AIL_RAMP_MS` = 3 ms**（33 样本 @11025）线性淡入/淡出；上限不超过样本长度的一半，短样本也安全。只改宿主副本，不动 guest 内存 |
| 残余可见 | 仍在播放时被重触发才算"硬切"，计入 `g_sample_cuts` 并在 `ail: shutdown` 行打印；单次播放日志加 `(cut)` 标记 |

**实证**：20 s 跑 `ail: play` 47 次、`(cut)` **0 次** —— 60 ms 的音效以 1.7 次/秒触发根本不会
重叠，所以**杂音的主因是起停阶跃（第 2 条），不是硬切**。这也说明斜坡修的是要害。

**未修 / 待办**：只剩硬切（重触发）。~~音乐侧的同类问题~~ 已在 §11.8 一并解决（流式合成后
`Sleep(200)` 重投造成的循环点阶跃不复存在）。治本是 §13.6 第 3 步 `audio_sokol.c` + `audio.h`：
sokol_audio 回调做软件混音，音乐与音效共用一条流，设备只开一次，斜坡在混音器里统一做。

### 11.7 音量策略：默认不衰减，调试才压低（2026-10-06）

`--volume=<0..100>` 只在 **waveOut 边界**衰减（数字样本衰减宿主侧 PCM 副本，音乐衰减渲染后的
播放缓冲），`--midi-dump` 的 WAV 与 `synth: rendered …` 统计仍按满量程，离线证据不受影响。

**默认 `100`** —— 也就是加 `--volume` 之前（第 31 轮之前根本不存在这个参数）的听感，游戏
自己的电平原样输出。第 31 轮把它定成 `10` 是为了"能在旁边干活不受打扰"，代价是**自己玩也
得手敲参数**，已改回。

| 场景 | 用法 |
|---|---|
| 自己玩 | 什么都不加（`ail: master output volume = 100%`） |
| 调试 / 无人值守 | 显式加 `--volume=10`（`regress.ps1` 已内置） |
| 静音但仍跑完整条流水线 | `--volume=0` |

改动点三处，改默认时必须一起改：`host.c` `g_volume`、`ail.c` `g_master_volume`、
`synth.c` `g_master`。

### 11.8 背景音乐改流式合成 + `AIL_set_sequence_volume` 的 `ms` 渐变（2026-10-06）

**症状**（用户在 DOSBox 原版里对比出来的）：进商店/酒馆一类场景后，之前正在播的背景音乐
会**慢慢 mute**，一直到剧情结束、进入战斗才重新起音乐。我们的移植**既不淡出也不停**，
从酒馆/道具店出来进战斗时，一直在放之前营地（或读档前）的曲子。

**两个独立缺口叠在一起**（缺一不可，只修哪个都没声音上的区别）：

1. **`AIL_set_sequence_volume(seq, vol, ms)` 把 `ms` 丢了**（`ail.c`）。
   真实 AIL 的语义是**在 `ms` 毫秒内从当前音量渐变到目标音量**（由 AIL 定时器驱动）。
   宿主日志里游戏确实在用它：

   ```
   ail: set_sequence_volume(0,   over 0 ms)      ← 换曲前先清零
   ail: set_sequence_volume(127, over 2000 ms)   ← 2 秒淡入
   ail: set_sequence_volume(0,   over 4000 ms)   ← 4 秒淡出  ← 用户看到的"慢慢 mute"
   ```

2. **就算实现了渐变也改不动音量**：播放链上没有实时增益。原来 `synth_play` 把**整首曲子一次
   渲染成一个大 buffer**，`g_master` 只在渲染时烘焙一次（旧注释自己写着 "the loop thread
   resubmits it unchanged"），`resubmit_thread` 每 200 ms 原样重投；而 `xmidi_set_volume` →
   `midi_set_volume`（CC7）只作用于 **MIDI Mapper**，对 `--midi-backend=winmidi` 以外的默认
   自带合成器路径毫无影响。⇒ **正在播的音乐的音量是一个常量。**

**修法：按原版 AIL 的架构改成流式合成**（AIL 的 MDI 驱动本来就是在定时器回调里增量渲染、
持续喂设备，音乐始终"在飞"，音量改动下一片就生效）：

| 改动 | 做法 |
|---|---|
| `stream_init()` / `stream_fill()` | 把渲染状态（采样时钟 `g_t`、事件游标 `g_ei`、voices）提到全局，可任意分片续渲染；`render()` 降级为"一片 = 整首"，只用于 `--midi-dump` 与渲染统计 |
| waveOut 分片队列 | `STREAM_SLICE` = 2048 样本（92 ms @22050）× `STREAM_BUFFERS` = 4（共 371 ms 预排队）， `stream_thread` 在 `WHDR_DONE` 时重新合成并回投 |
| 逐样本增益 | `stream_fill(dst, n, gain)`：`gain = 序列音量/127 × --volume/100`，在混音后、钳位前乘上 |
| `ms` 渐变 | `synth_set_sequence_volume(vol, ms)` 存 `from/to/起始时刻/时长`，`seq_volume_now()` 按墙钟插值；新请求从**当前渐变到的值**接着走，不会跳回 |

**为什么离线证据没变**：`--midi-dump` 的 WAV 与 `synth: rendered …` 统计仍走 `render()`
（整首、gain = 1.0、满量程），与改动前逐字节同级。实测回归里
`2256 notes … peak 32258/32767, polyphony 36` 与文档既有判据完全一致。

**实证**：

```
ail: set_sequence_volume(127, over 2000 ms) - ramped
synth: fading 0 -> 127 over 2000 ms: now  1.0/127 (  16 ms in)
synth: fading 0 -> 127 over 2000 ms: now 32.8/127 ( 516 ms in)
synth: fading 0 -> 127 over 2000 ms: now 64.5/127 (1016 ms in)
synth: fading 0 -> 127 over 2000 ms: now 96.3/127 (1516 ms in)
synth: streaming 4 x 2048-sample slices (92 ms each, 371 ms queued), sequence volume 127/127
```

（渐变每 500 ms 打一行，否则"渐变到底发生没有"只能靠听。淡出用的是同一条代码路径，
改动前的日志里已抓到过 `set_sequence_volume(0, over 4000 ms)` 这个调用。）

**顺带修掉**：音乐循环点不再是"整段播完 → `Sleep(200)` 轮询 → 重投"，而是连续的流式拼接，
§11.6 里那个"每个循环点一次静音→有声阶跃"的爆音源消失。

**未修**：`AIL_stop_sequence` 仍是硬切（`synth_stop` → `waveOutReset`）。不过游戏总是先
`ms` 淡到 0 再 stop，所以听感上已经是"淡完再停"。

**判据**：回归 **8/8 PASS**、`FD2.TMP = 207360`；`ail: play 16` / `(cut) 0`；
`synth: rendered` 统计与改动前一致。
