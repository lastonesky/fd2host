# 第 49 轮：地图世界叶子两件套（第 304–305 个）

> 优先级 2：把地图叶子放进**已有完整地图世界**的 `mapcheck`（`bitmap`/`nres`/`recs`/shape/CRT
> 重定向）里对拍。本轮收 `0x1DF58`（状态数字上浮）与 `0x1C2DA`（队伍图标 + 闪烁），
> 接入 303 → **305 / 1359（22.4%）**。

## 49.1 新增 2 个（并入 `src/game/map.c`，`REPL_MAP`）

| 地址 | 名字 | 作用 |
|---|---|---|
| `0x1DF58` | `map_draw_status_popups` | 22 帧把状态队列里的数字字形按上升行重画到地图位图，再 `gfx_copy_rows` 到 `0xA0504` |
| `0x1C2DA` | `map_draw_party_icons` | 把 `list` 里的记录各画一个 24×24 图标，再在当前/存档位图之间闪 5 次 |

`0x1DF58` 的目的地址是 `dword_53A49 + 0x8088 + 24*(x-ox) + 10944*(y-oy) + byte_53D34 + 456*(v3-3)`
（**`0x8088` 不是 `31536`，`v3-3` 也不是 `v3`**——后者使字形从下方升起）。
`0x1C2DA` 的图标取 `buf + *(u32*)(buf + 4*ebx)`，`ebx = 12*kind + (dword_53C0B==3 ? 2 : dword_53C0B)`，
`sub_4E127(src, dst, 456)` 的第 4 个 push（表项字节）被 `sprite24_const` 忽略。

## 49.2 `mapcheck` 扩展

- 新钩 `0x17AA9 svc_wait_ticks → no-op`（单线程 harness 里等 tick 会死循环）；
- 两个测试都把 `PTR(0x53A49)` 指向 `dstA`（原机器码）/`dstB`（C），逐字节比 400 KB 位图；
- 记录坐标夹在 13×8 视图内、`kind`/shape 索引夹在合法范围（`0x1DF58` **不做边界检查**，
  合成世界必须保证不越界）；
- `0x1DF58` 用 `dword_53A81`（nres 数字字形库），`0x1C2DA` 用 `dword_53A61`（ibank 头像库）。

`mapcheck` 全量 **128500/0**（含新 12 例）。

## 49.3 仍未接入：`0x1C4CC`

`0x1C4CC`（同类，带音效分支 + 前后 `map_view_update(0)`）的 C 也写好了，但**原机器码在
合成状态里段错误**（它对全刷新 `0x11CAC` 的全局前置更敏感）。等把 `0x11CAC` 的世界前置
补齐再收；**未对拍就不 wire**。

## 49.4 判据

```
mapcheck（全量，含新 12 例）：128500/0
regress：all 连续 3 次 8/8、none 8/8   repl: installed 305 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：305 wired / 1359（22.4%），0 translated-not-wired
```

> **记一次偶发**：本轮首次 `regress` 报 4 个文件断言失败、随后连续 3 次 8/8（含 `none`）
> ——签名与 `docs/PITFALLS.md` §8-85 的启动竞争一致，不是本轮代码回归。

## 49.5 下一步

1. 补齐 `0x11CAC` 的世界前置，收 `0x1C4CC`；顺带 `0x197E5`/`0x19953`/`0x1DB65`（场景绘制）。
2. `0x14818`/`0x12CEA`（路径揭示 + 视口滑动）。
3. `0x24618` + `0x22046`/`0x219AD`（堆 + 未转译依赖）。
