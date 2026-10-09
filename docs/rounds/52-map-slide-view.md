# 第 52 轮：`map_slide_view`（第 308 个）

> 第 51 轮因合成 world 下窗口 stepper 死循环而未接入的 `0x12CEA`，本轮定位根因后对拍接入。
> 307 → **308 / 1359（22.7%）**。

## 52.1 新增 1 个

| 地址 | 名字 | 作用 |
|---|---|---|
| `0x12CEA` | `map_slide_view` | `map_view_update(0)` 后把视口逐行滑到 `(target_x, target_y)`：每步调窗口 stepper（`0x11BFA/11C59` 走 x、`0x11B9B/11B48` 走 y），`dword_51A83` 既非 0 也非 6 时每步 `svc_wait_ticks(1)`，再 `kbd_flush()` |

## 52.2 根因：stepper 在地图边界不再移动

stepper `0x11BFA`（向右）的守卫是
```
if (dword_53AC1 - 1 != dword_53AB1) dword_53AB1 += 1;
```
即**到达地图右边界就不再推进**。测试若把 target 设成越界的 `current+1`，`while (target != dword_53AB1)`
永不收敛 → 死循环。修法：测试把 target 夹在 `[0, dword_53AC1-1]`、`[0, dword_53AC5-1]`；
另外 `setup_view()` 只设 `dword_53AC1`，**没有设 `dword_53AC5`**（y 向 stepper 的守卫用它），
测试里补上 `I32(0x53AC5)=32`。

> 一般化：把合成 target 喂给“带边界守卫的步进循环”前，**必须确认 target 在可达域内**，
> 否则 harness 死循环而不是报 FAIL。与 `docs/PITFALLS.md` §8-81 同类。

## 52.3 判据

```
mapcheck 全量（含新 6 例）：138500/0
regress：all 8/8          repl: installed 308 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：308 wired / 1359（22.7%），0 translated-not-wired
```

## 52.4 下一步

- 场景绘制 `0x197E5`/`0x19953`/`0x1DB65`（大屏幕缓冲；`0x19953` 是等键循环，
  需要 `0x10620 kbd_pending` 桩 + `0x370F0 int386` 确定性键盘）；
- `0x24618` + `0x22046`/`0x219AD`（堆 + 未转译依赖）；
- `0x2C67D`（`funcs_30469[6]`，含 CRT `cos/sin`）。
