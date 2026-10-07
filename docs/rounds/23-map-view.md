# 轮次明细：地图视图叶子四件套（第 66–69 个转译函数）
§53 继续按 `re/func_ranking.csv`（用量）取"热且依赖闭合"的 4 个：`map_blit_tile`(0x126F7, 37)、
`pal_fade_add`(0x11DF2, 34)、`res_blit6`(0x16886, 23)、`dlg_portrait_clear`(0x134E4, 25)。
四个的调用图里**没有未转译的游戏函数**（只碰已转译的 `sprite24_plain`/`rle_decode` 与服务
`outp`/`delay`/CRT），所以接进去就是纯 C 执行。`mapcheck` 25000/0，`fadecheck` 扩到 4 种模式。
---

## 53.1 这一批

| addr | 用法 | 转译 | 语义 |
|---|---|---|---|
| `0x126F7` | 37 | `map_blit_tile(x, y, index)` | 视图越界即丢；否则 `sprite24_plain(tileset + *(u32*)(set+4*idx+6), bitmap + (y-oy)*10944 + (x-ox)*24 + 32904, 456)` |
| `0x11DF2` | 34 | `pal_fade_add(start, end, add)` | `0x11D40` 的姊妹：每通道 **加** `add` 并**上限截到 0x3F**（6-bit DAC 满值） |
| `0x16886` | 23 | `res_blit6(dst, pitch, buf, index)` | LMI 子图 blit，但偏移表在 **+6** 且固定在 (0,0)：`rle_decode(buf + *(u32*)(buf+4*idx+6), 0, 0, dst, pitch, -1)` |
| `0x134E4` | 25 | `dlg_portrait_clear()` | 把 `dword_53BEB` 条头像记录（stride 80）的 byte+3（张嘴标志）清零，再 `delay(20)` |

新增 `src/game/map.c`（`map_blit_tile`，接 `--replace=map` 新分组）；`res_blit6` 放 `res.c`；
`pal_fade_add` 放 `fade.c`；`dlg_portrait_clear` 放 `dlg.c`。

**注意 `res_blit` 与 `res_blit6` 是两套偏移表**：`0x2EB9F` 用 `buf+8+4*idx` 且要传 x/y；
`0x16886` 用 `buf+6+4*idx` 且固定 (0,0)。别互相套用（`PITFALLS` §8-70 的姊妹坑）。

## 53.2 对拍

| 判据 | 结果 |
|---|---|
| `mapcheck`（新）| **25000 cases, 0 failures**：合成 tileset/ LMI 缓冲（24×24 全字面 token 流），
`map_blit_tile` 比对整幅位图（含视图内/左右/上下越界四类）、`res_blit6` 比对目标缓冲、
`dlg_portrait_clear` 比对标志位 + hook 到的 delay 次数 |
| `fadecheck` | **4000/0**（新增第 4 种模式：随机 `pal_fade_add`，含把通道顶到 0x3F 的截断） |
| `rescheck` | 160/0（`res.c` 变了，重跑） |
| `regress.ps1` | **8/8 PASS**，`FD2.TMP=207360` |
| `repl: installed` | **69**（mask 0x3FFF） |
| A/B 同 tick `--replace=none↔all` | **0 / 64000 px** |
| Linux（新分级） | `make` 0 warning、`letest` exact match、`doscheck` 49/49 |
| `translation_map` / `func_ranking` | **69 wired / 1359（5.1%）** |

## 53.3 下一步

`re/func_ranking.csv`（重跑后）下一档仍是"热点但依赖未清"的：`0x1366A`(110, 818 B)、
`0x135DD`(98)、`0x11CAC`(84)、`0x10B4E`(58)、`0x1956B`(52)…`0x135DD` 只差 `0x11CAC`；
`0x11CAC` 差 `0x11EEE`(885)/`0x122DC`/`0x127A9`(55)/`0x1297D`/`0x1ACF3`。
建议先按依赖拓扑把 `0x127A9`、`0x122DC`、`0x1297D`、`0x1ACF3` 这几个小的收掉，再上 `0x11CAC`，
然后 `0x135DD` 与 `0x1366A` 就自然可用。
