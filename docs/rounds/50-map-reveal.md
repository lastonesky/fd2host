# 第 50 轮：`map_reveal_reachable`（第 306 个）

> 继续把地图叶子放进 `mapcheck` 对拍。本轮收 `0x14818`（`map_reveal_reachable`），
> 接入 305 → **306 / 1359（22.5%）**。

## 50.1 新增 1 个

| 地址 | 名字 | 作用 |
|---|---|---|
| `0x14818` | `map_reveal_reachable` | 从 (x,y) 洪泛可达格（`range>=16` 走十字线清零，否则 `path_mark` 洪泛 + 可选菱形 reveal），再收集落在这些格上、按 `+6` 状态过滤的未标记记录，返回个数 |

（C 在第 45 轮写好，一直没 wire；本轮补上 `mapcheck` 测试后接入 `REPL_MAP`。）

## 50.2 关键点：对拍 world 的网格宽高在 `map[0]`/`map[2]`

第一版给 `dword_53AC1/53AC5=32`（这是**记录循环/十字线分支**用的），但 `path_mark`
（`0x4E390`）从**网格首字节**读宽高：`map[0]=W`、`map[2]=H`（见 `src/game/path.h` 的
map layout）。合成 world 里 `cells[0]`/`cells[2]` 是随机的，于是原机器码的洪泛按随机
宽高寻址、与 C 侧不一致（甚至越界）。把 `cells[0]=cells[2]=32` 后两侧完全一致。

> 一般化：**对拍 harness 喂给“读结构头”的函数时，头字段必须真的写进去**，
> 不能只设旁边那几个同义的全局。与 `docs/PITFALLS.md` §8-81（哨兵坐标越界）同类。

## 50.3 判据

```
mapcheck 全量（含新 8 例）：132500/0
regress：all 8/8          repl: installed 306 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：306 wired / 1359（22.5%），0 translated-not-wired
```

## 50.4 下一步

- `0x1C4CC`（同类，带音效 + `map_view_update(0)`）与 `0x12CEA`（`map_slide_view`，视口滑动）
  都要先把 `mapcheck` 的**完整视图 world**（`setup_view` 那套）铺进测试再收；
- 场景绘制 `0x197E5`/`0x19953`/`0x1DB65`（大屏幕缓冲）；
- `0x24618` + `0x22046`/`0x219AD`（堆 + 未转译依赖）。
