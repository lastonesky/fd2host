# 轮次明细：主状态机族第一刀 —— `0x25977` play_bgm 转译（第 55 个转译函数）
§49 开始啃 `re/RE_MAP.md` 标为 ★★★ 的"主状态机"一族。第一步是 IDA 侦察：发现其中
`0x25977` 其实是**换曲入口 `play_bgm`**（不是状态机本体），32 个调用点、只调服务不调游戏逻辑 ——
适合当这一族的第一刀。转译 + 差分对拍当场抓到一个真语义差异（`-1` 的比较）。
---

## 49.1 侦察结论（IDA，`E:\FD2\FD2.EXE.i64`）

| 地址 | 大小 | 调用点 | 是什么 |
|---|---|---|---|
| **`0x25977`** | 287 B | **32** | **`play_bgm(track, loop_count)`** —— 唯一换曲入口；调 `res_load("FDMUS.DAT",…)` + AIL 序列族 + `0x3666C`(DPMI lock) |
| `0x25EBB` | 663 B | 1 | 与 `funcs_25E23[]`/`funcs_25E3A[]` 两张函数指针表配合的状态处理 |
| `0x117E7` | 705 B | 1 | 待测 |
| `0x22E5C` | 154 B | 1 | 待测 |
| `0x26152` | 1178 B | 2 | 待测 |

`0x3666C(buf,size)` → `sub_365DA` → `int386(0x31, {0x0600, …})` = **DPMI `LOCK_LINEAR_REGION`**，
在平坦宿主里是 no-op 服务（保留调用以便对拍与将来）。

> `re/RE_MAP.md` 原来把 `0x25977` 归到"主状态机"里，本轮更正为 `play_bgm`。

## 49.2 转译

- `src/game/bgm.c` + `bgm.h`：`void bgm_play(int track, int loop_count)`；读 `byte_51A11`（当前曲）、
  `byte_53EF0`（音乐开关）、`byte_51E61`（淡入开关）、`dword_53ED0`（AIL 句柄）、`dword_53EE0`
  （FDMUS 缓冲）；**服务一律经原地址调用**（`0x111BA` res_load、`0x3666C` lock、
  `0x3ADF5/0x3AEEE/0x3AF5B/0x3B124/0x3B1A6` 五个 AIL 序列入口）——宿主里它们已被 `ail.c`/`res.c`
  替换，对拍里被 hook 成记录桩，两边走同一条路。
- `src/repl.c` 新分组 `bgm`（`REPL_BGM 0x400`），接入第 **55** 个函数。
- 淡入语义（0 ms 静音 → 127 over 2000 ms；曲 16/17 直接满音量）与 `docs/rounds/06-audio-fade.md`
  记录的原版行为一字不差。

## 49.3 对拍抓到真 bug（这就是要写 `*check` 的原因）

`0x25977` 开头是：

```asm
movzx eax, byte_51A11
cmp   eax, [esp+4+arg_0]      ; 零扩展字节 vs 完整 32 位实参
jz    ret
```

我第一版写成 `if (byte_51A11 == (uint8_t)track) return;` —— 对 `track == -1`，C 里
`0xFF == 0xFF` 成立而**直接返回**，机器码却是 `0xFF != 0xFFFFFFFF` **继续执行**（发淡出）。
`bgmcheck` 第 81 例（`track=-1, byte_51A11=0xFF`）当场报 `event count 1/0`。
修法：比较保持整数（`(unsigned)byte_51A11 == (unsigned)track`），只有**写入**时才截断成字节。
已记 `PITFALLS` §8-69。

## 49.4 判据（本轮实测）

| 判据 | 结果 |
|---|---|
| `bgmcheck` | **6000 cases, 0 failures**（20 轮 × 300：事件序列 + 全局；覆盖 `track∈{-1,0..22}`、`byte_51A11` 命中/不命中、音乐/淡入开关、有无旧缓冲） |
| `regress.ps1` | **8/8 PASS**，`FD2.TMP=207360` |
| `repl: installed` | **55**（mask 0x7FF）；日志仍有 `ail: set_sequence_volume(127, over 2000 ms) - ramped`（证明换曲走的是 C） |
| A/B 同 tick `--replace=none↔all` | **0 / 64000 px** |
| Linux `host32` 同 tick ↔ Windows | **0 / 64000 px** |
| `translation_map.py --check` | **55 wired / 1359（4.0%）** |

## 49.5 下一步

`0x25EBB`/`0x117E7`/`0x22E5C`/`0x26152` 是本族剩余的状态机本体；它们互相调用、依赖
`funcs_25E23[]`/`funcs_25E3A[]` 两张函数指针表与大量全局。建议下一个取 **`0x22E5C`（154 B，最小）**
先把表项与状态变量摸清，再向上做 `0x25EBB`。记录维持：`repl.c` 加行 →
`python tools/translation_map.py`。
