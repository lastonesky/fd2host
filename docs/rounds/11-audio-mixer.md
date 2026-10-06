# 轮次明细：音频治本 —— 一个设备、一个软件混音器
§41 把音乐（`synth.c`）与音效（`ail.c`）从各自为政的 waveOut 收进 `audio.h` + `audio_sokol.c`
（WASAPI），并给音频第一次配上**可测量**的判据（`--audio-dump`）。
---

## 41. 第 41 轮：软件混音器（2026-10-06）

### 41.1 起点问题（`BACKEND` §13.6 第 3 步，长期挂账）

```
音乐  synth.c → 自己的 HWAVEOUT（分片流，独立线程）
音效  ail.c   → 每个 sample handle 自己的 HWAVEOUT（常驻，格式变就重开）
```

两条通道各开各的设备 ⇒ 每次设备启停都是潜在爆音源、`--volume`/斜坡要在两边各做一遍、
"还有声音吗"没有一个统一的观测点；而且 `waveOut` 是 Windows 专用，**跨平台绕不开**。
`AUDIO.md` §11.6/§11.8 的"未修"清单里，硬切、重触发 cut、双设备都指向这里。

### 41.2 结构

```
                    ┌────────────── src/audio.h ──────────────┐
 synth.c  音乐分片 → │  mix_cb(WASAPI 回调, 一次 2048 帧)      │ → 唯一设备
 ail.c    音效人声 → │   out = music + Σ sfx voices            │
                    └──────────────────────────────────────────┘
```

| 改动 | 做法 |
|---|---|
| `src/audio.h` | 接口：`audio_init/close`、音乐源 `audio_set_music/hold/release`、`audio_sfx_play/stop/active`、递归锁、`audio_dump_open` |
| `src/audio_sokol.c` | **sokol_audio（WASAPI）pull 回调**，不依赖渲染后端 ⇒ GDI/sokol 两套入口共用；一把递归 `CRITICAL_SECTION` 护住全部音频状态；人声 `float` 单声道、线性重采样（人声 11025 → 设备 22050）、跑完即释放 |
| `synth.c` | 删掉 `stream_thread`/`waveOut`/4 个缓冲；改成**回调按需拉取** `synth_music_fill()`（增益在拉的那一刻算 ⇒ 音量改动零延迟）；起播闸门改用 `audio_hold/release_music()`；`fading` 日志跟着搬进填充函数 |
| `ail.c` | 删掉每句柄的设备（`sample_open_device`/`waveOutWrite`/`WHDR_DONE` 轮询）；`sample_play` 把**已经烘焙好增益+3 ms 斜坡**的 PCM 副本交给混音器并交出所有权；`sample_stop` = 丢人声（仍在响才算 `(cut)`） |
| `host.c` | `audio_init` 在游戏线程起来前调用；新增 `--audio-rate=`（默认 22050 = 音乐原生采样率）与 `--audio-dump=`；**watchdog 的 `ExitProcess` 路径也要 `audio_close()`**（见 §8-56） |
| `xmidi.c` | `synth_play(..., audio_rate())` ⇒ **音乐按设备采样率渲染，混音器不必重采样音乐**（音色时值不变，只有采样率变） |

**保留不动的**：增益仍烘焙在上游（音乐 = 序列音量 × `--volume`；音效 = `--volume` ×
AIL 音量 + 3 ms 起停斜坡），混音器只做相加 —— 这样 §11.6/§11.7/§11.8/§11.9 的所有音量
语义**逐位不变**，风险最小。

**一个设计约束**：`saudio_setup()` 只能调一次（`SOKOL_ASSERT(!_saudio.setup_called)`），
所以采样率不能"试几个"，只能由 `--audio-rate` 显式指定；默认 22050 能被 WASAPI 接受是因为
sokol 用 `AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM` 初始化（共享模式自动转换）。

### 41.3 判据（音频第一次有了可测量的证据）

| 层 | 判据 | 结果 |
|---|---|---|
| 设备 | `audio: device up - WASAPI via sokol_audio, 22050 Hz, mono, 2048 frames/buffer (opened once…)` | **每进程只出现 1 次**（= 设备只开一次） |
| 流动 | `audio: mixed N frames …` 每 10 s 一行 | 225280 → 446464 → 667648，**单调前进** |
| 分声道响度 | 同一行拆成 `music=on peak …/32767` 与 `sfx voices=… peak …/32767` | music peak 1149/1380、sfx peak **255** ⇒ 两路都在往设备送非静音数据 |
| 离线可测 | **`--audio-dump=<wav>`**：把混音器交给设备的那串样本录下来 | 30.1 s / 663552 帧，文件大小 = 44 + n×2 精确 |
| 离线量化 | `mix.wav` 逐秒 RMS | **0–1 s = 0**（起播闸门 + 从 0 淡入）、2 s 起连续非零 30 s、稳态 RMS 1.2% FS @`--volume=10` |
| 音量语义 | 同一段用 `--volume=100` 再录一次 | 稳态 RMS 396 → **4081（10.3×）** ⇒ `--volume` 语义经重构后保持 |
| 音效语义 | `ail: play` 次数与 `(cut)` 计数 | **16 / 0**（与 §11.6、§11.9 判据一致） |
| 回归 | `regress.ps1` | **ALL PASS（8/8）**、`FD2.TMP = 207360` |
| 像素 | `--replace=none` vs `all`，`--shot-tick=600` | **0 / 64000 px**（音频改动不影响画面） |

### 41.4 这一轮**没有**动的东西（明确记下，避免以后误判）

- `--midi-dump` 的 WAV 与 `synth: rendered …` 统计仍走离线 `render()`、满量程、
  `gain = 1.0` ⇒ 判据数字不变（`2256 notes … peak 32258/32767, polyphony 36`）。
- 起播闸门、序列音量 `ms` 渐变、SFX 3 ms 斜坡、`(cut)` 计数：**全部保留原语义**，
  只是承载点从 waveOut 换成混音器。
- MIDI Mapper 后端（`--midi-backend=winmidi`）不动。

### 41.5 踩到并记档的坑

**watchdog 路径绕过 `host_shutdown`**（`PITFALLS` §8-56）：`--exit-after`/`--exit-when-file`
到点后由独立线程直接 `ExitProcess(0)`，`host_shutdown()` 根本不跑 —— 以前只是"设备没被关"
（OS 收拾），现在 `--audio-dump` 的 WAV 头拿不到最终长度就变成坏文件。修法：在那条退出路径上
`ExitProcess` 之前调 `audio_close()`。

### 41.6 下轮入口

`PROGRESS.md` 下一步第 4 条**收口**。剩下的：第 5 条（稳定性长跑）、第 6 条（存档路径，
只在沙箱）、第 7 条（跨平台 —— `audio.h` 现在是纯接口，sokol_audio 在 Linux 是 ALSA ✓）。
