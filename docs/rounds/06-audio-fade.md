# 轮次明细：音量排查（场景/进游戏"先小后大"）
§35 每个场景、刚进游戏时音量"先小一下，再大回去"的排查 + 排查中发现并修掉的起播抢跑。
---

## 35. 第 35 轮：音量"先小后大"排查（2026-10-06）

### 35.1 症状

用户报告：**加了静音（fade/mute）代码之后**，任何场景（**包括刚进游戏**）音量似乎都会
先小一下、再大回去。怀疑是第 §11.8（commit `5f38f1d`，"补 `AIL_set_sequence_volume` 的
`ms` 渐变"）引入的回归。

### 35.2 证据链（四级，全部硬证据）

**① 运行日志**（`port/build/host.log`，每次换曲都出现同一对调用）：

```
synth: streaming 4 x 2048-sample slices … sequence volume 127/127
ail: set_sequence_volume(0,   over 0 ms)       ← 游戏先把曲子静音
ail: set_sequence_volume(127, over 2000 ms) - ramped
synth: fading 0 -> 127 over 2000 ms: now  1.0/127 (  16 ms in)
synth: fading 0 -> 127 over 2000 ms: now 32.8/127 ( 516 ms in)
synth: fading 0 -> 127 over 2000 ms: now 64.5/127 (1016 ms in)
synth: fading 0 -> 127 over 2000 ms: now 96.3/127 (1516 ms in)
```

**"先小再大"就是这两行调用：0 毫秒静音 → 2 秒淡入**。听感上= 每首曲子开头 2 秒小声。

**② 这是游戏自己写的，不是我们加的** —— IDA 反编译 `play_bgm`（`0x25977`，唯一换曲入口）：

```c
AIL_init_sequence(dword_53ED0, dword_53EE0, 0);
AIL_start_sequence(dword_53ED0);
if ( byte_51E61 != 0 ) {                       /* 音乐开关打开 */
    if ( a5 == 16 || a5 == 17 )                /* 胜利曲等：立刻满音量 */
        v7 = 0;
    else {
        AIL_set_sequence_volume(dword_53ED0, 0, 0);     /* 换曲先清零     */
        v7 = 2000;                                       /* 然后 2 秒淡入  */
    }
    AIL_set_sequence_volume(dword_53ED0, 127, v7);
} else
    AIL_set_sequence_volume(dword_53ED0, 0, 0);
AIL_set_sequence_loop_count(dword_53ED0, a6);
```

调用点还有 `sub_1728C`（选项菜单开关音乐：`set(127,1000)` / `set(0,1000)`）。
**游戏只对 track 16/17 走"立刻满音量"，其余每首都淡入** —— 这是原作有意的编排。

**③ 原版 AIL 真的会按 `ms` 淡**（同一份 `FD2.EXE` 里的 AIL V3.02，反编译钉死）：

| 位置 | 干什么 |
|---|---|
| `AIL_set_sequence_volume` `0x3B124` → `sub_449E0` | `a1[14]=目标`；`ms==0` 直接 `a1[13]=目标`（瞬时）；否则 `a1[16] = 1000*ms/|当前-目标|`（每 1 级音量多少 µs） |
| MDI 驱动 service `sub_43270` | 每 tick 累加 `driver+16`（µs），够一个 step 就把 `a1[13]` ±1 直到等于目标；`(计数器 & 7)==0` 时 `sub_43230` 重发 CC7 |
| 发送函数 `sub_42980` | 遇到 CC7：`value = value * a1[13] / 127`（钳 0..127）—— **序列音量是乘在每个 CC7 上的** |

⇒ 原版就是**在 `ms` 毫秒内线性淡变**，总时长精确等于 `ms`（2000 ms / 4000 ms 各自对应
游戏里的 2 秒淡入 / 4 秒淡出）。我们的 `seq_volume_now()` 按墙钟插值，等价。

**④ 原版的淡变能波及整条混音吗？**（因为原版只乘在 CC7 上，没发过 CC7 的声道不受影响）
新增 `tools/xmi_cc7.py`，扫 `FDMUS.DAT` 里全部 15 首 XMIDI 的 `EVNT`：

```
python tools/xmi_cc7.py build/aildump/fdmus_*.bin
```

结果：**每首曲子所有出声声道都至少有一条 CC7**（唯二 "NO CC7" 一个是 1 个音符的杂声道、
一个是 0 音符的空声道）。⇒ 原版的淡入淡出同样覆盖全部旋律/鼓道，和我们"整条音乐混音乘增益"
**听感等价**。

### 35.3 结论

**不是回归，是原作自己的 2 秒淡入**。第 §11.8 之前 `ms` 被丢掉（`host_AIL_set_sequence_volume`
只打印、直接把音量当常量用），所以曲子一上来就是满音量 —— **那才是偏离原作的状态**；
现在听到了淡入，说明 `ms` 生效了。判据：`docs/AUDIO.md` §11.9、本文件 ②③④ 三级证据。

顺带记一条判据：**"听感对不对"不能只靠听** —— 本轮把"游戏要什么"（②）、"原版怎么实现"（③）、
"淡变覆盖哪些声道"（④）三条独立证据互相钉，才敢下"忠实"的结论。

### 35.4 排查中挖出来的**移植侧**偏差：起播 371 ms 抢跑（已修）

`synth_play` 原来在 `AIL_start_sequence` **内部**就把 4 × 2048 样本（**371 ms**）填好、
`waveOutWrite` 送出去了；而游戏是在 `start` 返回**之后**微秒级就发 `set(0,0)`。
于是新曲开头 371 ms 吃到的是**上一首留下的增益**（通常是 127/127）：

> 满音量爆一下 → 掉到淡入起点 → 2 秒后回到满音量

原版 AIL 由定时器中断驱动，`set(0,0)` 在第一个样本送进设备**之前**就生效，没有这个窗口。
实证抢跑有内容：`--midi-dump` 离线渲染标题曲，**第一个音出现在 0.2 ms**、
**前 371 ms 峰值 17709/32767**（`build/title.wav`）—— 不是静音，是实打实的满电平开头。

**修法（`src/synth.c`，加"起播闸门"）**：`synth_play` 只 `PrepareHeader` 不 `Write`，
`g_arm=1` 让 `stream_thread` 停在门口；**第一条 `synth_set_sequence_volume` 才放行**
（`STREAM_ARM_MS=200` 超时兜底，防某个调用方永远不设音量）。放行时打印当时的增益：

```
synth: streaming 4 x 2048-sample slices …, first queue held for the volume request
ail: set_sequence_volume(0, over 0 ms)
synth: stream released after 0 ms -> sequence volume 0/127, gain 0.000   ← 首队列按 0 增益填
ail: set_sequence_volume(127, over 2000 ms) - ramped
```

`gain 0.000` 就是硬判据：**首批 371 ms 是静音，不再抢跑**，之后按游戏的 ramp 走，时间轴与原版一致。

连带处理的两个细节：
- `synth_stop()` 里"等 `WHDR_DONE`"对**从未 Write 过**的 header 永远不成立 ⇒ 每片空转 1 s、
  整段停曲卡 4 s。改成只等 `g_queued[i]` 的片（`PITFALLS` §8-52）。
- `g_arm` 在 `synth_stop()` 里清零，防止上一条流的闸门状态泄漏到下一条。

### 35.5 判据

| 项 | 结果 |
|---|---|
| `pwsh -File port\regress.ps1` | **ALL PASS（8/8）**，`FD2.TMP = 207360`（= 原件） |
| `ail: play` / `(cut)` | **16 / 0**（与 §11.6 判据一致） |
| `synth: rendered …` 离线统计 | `2256 notes … peak 32258/32767, polyphony 36` **与 §11.8 逐字一致**（离线路径没动） |
| 起播闸门 | `stream released after 0 ms -> … gain 0.000` |
| 淡入仍按游戏请求发生 | `synth: fading 0 -> 127 over 2000 ms` 每 500 ms 一行，4 行到顶 |

### 35.6 下轮入口

音量侧本轮收口；`docs/AUDIO.md` §11.9 是结论卡。剩下的音频待办仍是 §11.7 的"治本"
（`audio_sokol.c` 软件混音，音乐+音效共用一条流）—— 见 `PROGRESS.md` 下一步第 4 条。
源码化主线（`sub_15F84` 脚本 VM）见 `docs/TRANSLATION.md` §5 第 1 条。

### 35.7 回归 + 再修：闸门把音乐整个堵死（2026-10-06 晚，用户实听发现）

**症状**：这一版**音乐没了，只剩音效**。

**根因**（`src/synth.c`，日志取证 `hdr flags 0x2`）：`stream_thread` 判断"这个缓冲可以重填"
用的是 `WHDR_DONE`。闸门期间缓冲只 `waveOutPrepareHeader`、从未 `waveOutWrite`，而
**`PrepareHeader` 只置 `WHDR_PREPARED`(0x2)，永远不置 `WHDR_DONE`(0x1)** ⇒ 闸门放行之后
`if (!(dwFlags & WHDR_DONE)) continue;` 照样命中，**4 个缓冲一个都没进 waveOut**，
音乐线程空转（`fading` 行照打，因为那几行在填充循环之外）。SFX 走自己那条设备路径，所以照常。

**为什么上一轮没抓到**：判据只做到 `stream released … gain 0.000` + `fading 0 -> 127 …`
—— 这两行只证明"**按什么电平去填缓冲**"，**没有证明缓冲进过设备**。更刺的是
`PITFALLS §8-52` 当时已经写下"从未 `Write` 过的 header 永远不会置 `WHDR_DONE`"，
但只用在了 `synth_stop` 的等待上，没回头改 `stream_thread` 的就绪判据。

**修法**：就绪判据改成"**没进过队列的缓冲本来就是我的**"——

```c
if (g_queued[i] && !(g_hdrs[i].dwFlags & WHDR_DONE))   /* 只有驱动拥有的才等 DONE */
    continue;
```

**判据升级（本轮教训）**：音频判据必须**到设备这一层**，新增两行：

| 新判据行 | 证明什么 |
|---|---|
| `… first queue held for the volume request (hdr flags 0x2)` | 取证：`Prepare` 后只有 `0x2`，`DONE` 没置（根因） |
| `synth: stream alive - N slices, pos P (type T), gain G, queued peak X/32767`（每 10 s） | N 单调增长 + pos 前进 = **设备在消费**；`peak > 0` = **内容非静音**；G = 当前电平 |

**实测**（`--volume=10`，40 s 跑）：

```
synth: streaming … first queue held for the volume request (hdr flags 0x2)
synth: stream released after 0 ms -> sequence volume 0/127, gain 0.000
synth: stream alive - 4 slices,   pos 0        (type 4), gain 0.001, queued peak   14/32767
synth: stream alive - 111 slices, pos 440750   (type 4), gain 0.100, queued peak 1108/32767
synth: stream alive - 219 slices, pos 881838   (type 4), gain 0.100, queued peak 1174/32767
synth: stream alive - 326 slices, pos 1322198  (type 4), gain 0.100, queued peak  740/32767
```

108 片/10 s × 92 ms ≈ 10 s（实时）、`type 4` = `TIME_BYTES` 44100 B/s = 22050 Hz × 2 B
（驱动不支持 `TIME_MS` 时回退），都说明**数据在实时流出设备**。
回归 **8/8 PASS**、`FD2.TMP = 207360`。坑记 `PITFALLS.md` §8-54。
