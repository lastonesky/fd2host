# 第 51 轮：`map_draw_party_icons_anim`（第 307 个）

> 把第 49 轮写好、因合成世界不足而段错误的 `0x1C4CC` 用 `mapcheck` 的**完整视图 world**
> （`setup_view`：bitmap/tileset/nres/sbank/pbank/cells + 视图原点 + 调色板动画态）对拍后接入。
> 306 → **307 / 1359（22.6%）**。

## 51.1 新增 1 个

| 地址 | 名字 | 作用 |
|---|---|---|
| `0x1C4CC` | `map_draw_party_icons_anim` | 前后各 `map_view_update(0)`；逐帧从 shape 库 `0x53AD1` 取子图透明贴到记录格，按 `table_idx`/帧号触发音效，`page/loop` 由 `byte_51F33/51F54/51F75` 三张 33 字节表控制 |

## 51.2 关键点

1. **完整视图 world 是前提**：`0x1C4CC` 一进来就 `map_view_update(0)`，它要读视图原点、
   tileset、nres、cell 表、调色板动画态等一大堆全局；第 49 轮只铺了记录/位图 → 原机器码段错误。
   本轮直接复用 `mapcheck` 的 `setup_view()`（两次调用、`g_rnd` 恢复到同一起点）。
2. **shape 库是 `0x53AD1`（+6 偏移表）**，不是 `0x53A61`/`0x53A81`；测试把它指向 `nres`。
3. **`sub_4E127(src,dst,456)` 的第 4 个 push 被忽略**（`sprite24_const` 只读 3 参，`0x1C2DA` 同理）。
4. 比对位图 + VGA + cell 表 + 记录表，`--cases` 全量 **135500/0**（含新 6 例）。

## 51.3 未接入：`0x12CEA map_slide_view`

C 已写好（`map.c`），但**在合成 world 里死循环**：它 `while (target_x != dword_53AB1)` 反复调
窗口 stepper（`0x11B48/9B/BFA/C59`），而这些 stepper 在测试世界下不改变 `dword_53AB1`
⇒ 永不收敛。要收它需要先让 stepper 真能推进视口（或给循环设上限的专用 world）。
**未对拍就不 wire**。

## 51.4 判据

```
mapcheck 全量（含新 6 例）：135500/0
regress：all 8/8          repl: installed 307 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：307 wired / 1359（22.6%），0 translated-not-wired
```

## 51.5 下一步

- `0x12CEA`（视口滑动）需先解决 stepper 在合成 world 下不推进的问题；
- 场景绘制 `0x197E5`/`0x19953`/`0x1DB65`（大屏幕缓冲 + 等键循环）；
- `0x24618` + `0x22046`/`0x219AD`（堆 + 未转译依赖）。
