# 轮次明细：动画/地图格/头像查找四件套（第 70–73 个转译函数）
§54 继续按 `re/func_ranking.csv` 沿 `0x11CAC` 的依赖拓扑往下收：`0x1297D`(动画帧计数)、
`0x12E38`(地图格信息)、`0x12C0D`(找头像记录)、`0x4EB48`(0x627D8 指针表)。四个都是**全闭合**的
（只碰已转译的 `rec_flag`/`sprite24_*` 与服务 `outp`/`delay`/BDA 镜像）。
---

## 54.1 这一批

| addr | 用法 | 转译 | 语义 |
|---|---|---|---|
| `0x1297D` | 13 | `anim_frame_step()` | `tick=(int16)BDA[0x46C]`；`tick-dword_53C0F>4` 或回绕时 `++dword_53C0B%4` 并记 tick；随后 `++dword_53C07%4` |
| `0x12E38` | 22 | `map_cell_info(x,y,out)` | `tile = cell[+4]&0x3FF`、`flags = cell[6]&0x1F`；`out[0..3]=tile,flags`，`out[4..7] = dword_53A69[4*tile..]` |
| `0x12C0D` | 9 | `dlg_portrait_find()` | 遍历 `dword_53BEB` 条头像记录：`p[0]｜p[1]<<32 == qword_53AB1` 且 `rec_flag(i)==0` → 返回 i，否则 -1 |
| `0x4EB48` | 1 | `tbl_off627D8(i)` | `*(void**)(0x627D8 + 4*i)` |

新增 `src/game/anim.c`（`anim_frame_step`，读 BDA tick 走 `DOS_LOWMEM_BASE`，与 `kbd.c` 同法）；
`map_cell_info` 进 `map.c`；`dlg_portrait_find` 进 `dlg.c`；`tbl_off627D8` 进 `tables.c`。

## 54.2 对拍（`mapcheck` 扩展）

`mapcheck` 现在也做**低内存操作数重定向**（`0x46C` → `0x70000` 镜像），新增 4 组用例：

| 函数 | 判据 |
|---|---|
| `anim_frame_step` | 随机 tick / dword_53C0F（含"刚过 4 tick"与"回绕(负差值)"两类边界）、计数器初值 → 三个全局逐项一致 |
| `map_cell_info` | 随机地图宽/坐标/格字节（故意把 tile 高位塞满以走 `&0x3FF`）→ 8 字节输出一致 |
| `dlg_portrait_find` | 随机记录表 + 命中/未命中 → 返回值一致（`rec_flag` 为 C，两run同源） |
| `tbl_off627D8` | 表项指针一致 |

结果 **55500 cases, 0 failures**。整套：`regress` **8/8**、`repl: installed 73`（mask 0x3FFF）、
A/B 同 tick **0 px**、Linux `make`+`letest`+`doscheck` 全过（新分级，未拍图）。
进度 **73 / 1359（5.4%）**。

## 54.3 下一步（仍朝 `0x11CAC`）

`0x11CAC` 的依赖还剩：`0x11EEE`(885，差 `0x24D22`)、`0x122DC`(1051，只差已转译的 `map_blit_tile`)、
`0x127A9`(55，差 `0x129EC`；`0x129EC` 差 `0x12AC6`/`0x1F183`)、`0x1ACF3`(446，差
`0x12C0D`✔/`0x12E38`✔/`0x1875D`/`0x1AEB1`)、`0x4E31C`(101)。
⇒ 下一批建议：`0x187D6`(数字渲染) → `0x1875D`/`0x1AEB1`(依赖它) → `0x129EC`/`0x12AC6`/`0x1F183`
→ `0x127A9`；然后 `0x122DC`、`0x24D22`、`0x4E31C`，最后 `0x11EEE` → `0x11CAC` → `0x135DD` → `0x1366A`。
