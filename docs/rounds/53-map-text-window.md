# 第 53 轮：`map_draw_text_window`（第 309 个）

> 场景绘制三件套的第一个：`0x197E5`。用 `mapcheck` 的完整视图 world 对拍后接入。
> 308 → **309 / 1359（22.7%）**。

## 53.1 新增 1 个

| 地址 | 名字 | 作用 |
|---|---|---|
| `0x197E5` | `map_draw_text_window` | 先（cell 表存在时）`anim_frame_step` + `map_render_view` + `dlg_portraits_refresh`；然后 4 帧：从 staging 屏 `0x53C63` 拷一条 310 字节带进地图位图、用 `gfx_blit_transparent` 把两个 raw shape 块按滑动偏移 -12..0 / 0..12 贴上、`gfx_copy_rows` 到 `0xA0504`；最后把下带拷回 `0xA8C05` |

## 53.2 关键点

1. **`0x53A89` 的 shape 表是 12 字节步长**，条目值是 `bank + 12*idx` 处的 32 位偏移；
   `unk_51EE5=16`、`unk_51EE9=17` 是固定的两个 shape 索引（直接从加载后的 obj1 读，不硬编码）。
2. **`0x4ED34` 是 `gfx_blit_transparent`（raw `[u16 w][u16 h][pixels]`），不是 RLE**：
   测试里 shape 块按 `u16 w=24,h=24 + 576 字节像素` 造。
3. staging 屏 `0x53C63` 在 `mapcheck` 里没设，测试补上（`scrB`，内容用与 rnd 无关的确定模式，
   保证两侧起点一致）；位图基址 `0x53A49 + 0x1A59C` 需要 400KB 缓冲。

## 53.3 判据

```
mapcheck 全量（含新 6 例）：141500/0
regress：all 8/8          repl: installed 309 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：309 wired / 1359（22.7%），0 translated-not-wired
```

## 53.4 下一步

- 场景绘制余下 `0x19953`（等键循环：`0x10620 kbd_pending` 桩 + `0x370F0 int386` 确定性键盘）
  与 `0x1DB65`（堆用户）；
- `0x24618` + `0x22046`/`0x219AD`（堆 + 未转译依赖）；
- `0x2C67D`（`funcs_30469[6]`，含 CRT `cos/sin`）。
