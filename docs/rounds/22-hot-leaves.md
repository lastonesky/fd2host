# 轮次明细：热叶子六件套（第 60–65 个转译函数）
§52 按 `re/func_ranking.csv` 的用量排序，取了前 6 个"热且是叶子"的函数一次做掉：
`kbd_flush`/`kbd_pending`（BDA）、`util_rand`、`gfx_copy_rows`、`res_blit`、`dlg_portrait_glide`。
合计约 **350 个调用点**；`leafcheck` **72008/0**。顺带记一个 `rle_decode` 目的地语义的坑。
---

## 52.1 这一批（按用量顺序）

| addr | 用法数 | 转译 | 语义 |
|---|---|---|---|
| `0x4E381` | 64 | `kbd_flush()` | `BDA[0x41C] = BDA[0x41A]`（丢待取键） |
| `0x4EBE3` | 40 | `util_rand()` | `ax=word_627B8; ax+=0x9014; rol ax,1 ×3; 存回; 返回零扩展的 ax` |
| `0x10620` | 17 | `kbd_pending()` | `BDA[0x41C] != BDA[0x41A]` |
| `0x11EB0` | 115 | `gfx_copy_rows()` | `rows` 行、每行 `len` 字节、两套 stride 的 `memmove` 循环 |
| `0x2EB9F` | 82 | `res_blit()` | 按 `buf+8+4*index` 取子图头（w,h）→ `rle_decode(hdr+9, w, h, dst, pitch, mode)` |
| `0x12D7B` | 28 | `dlg_portrait_glide()` | 读 `dword_53A45 + 80*idx` 的前两字节 → 调 `0x12CEA`（人像滑入） |

`kbd.c` 是新增文件（BDA 两个）：宿主的 BDA 在 `DOS_LOWMEM_BASE` 镜像里，`dos.c` 把游戏里
写死的 `0x41A/0x41C` 立即数重定向到那里，C 读同一个镜像。`res_blit` 放进 `res.c`（调用已
对拍的 `rle_decode`，所以 `rescheck` 也补链了 `game/rle.c`）。

## 52.2 对拍：`src/leafcheck.c`（一个 harness 覆盖 6 个）

| 函数 | 判据 |
|---|---|
| `kbd_flush`/`kbd_pending` | 复用 keycheck 的**低内存操作数重定向**（`0x41A/0x41C` → `0x70000` 镜像），随机顶点跑原机器码 vs C |
| `util_rand` | 随机种子 → 返回值 + `word_627B8` 都要一致 |
| `gfx_copy_rows` | 随机 stride/len/rows，目标缓冲**逐字节**比较 |
| `res_blit` | **真实 `FDOTHER.DAT` 第 79 号资源**（场景卡那张）的全部子图 × 4 种 mode，原机器码 `rle_decode` vs C `rle_decode`，目标 VGA 大小缓冲逐字节比 |
| `dlg_portrait_glide` | hook `0x12CEA`，比较 `(x,y)` 与调用次数 |

结果 **72008 cases, 0 failures**。

## 52.3 `rle_decode` 目的地语义（harness 先踩的坑）

第一版 `res_blit` 用例给目标缓冲只开了 `320*h + 4096`（`h` 取子图**头**里的高度），结果比较到
`@166` 就报差异。原因是：

```asm
04E9A6  mov ecx,[ebp+arg_4]      ; x
04E9A9  mov eax,[ebp+arg_8]      ; y
04E9AC  mov edi,[ebp+arg_C]      ; dst
04E9AF  mov edx,[ebp+arg_10]     ; pitch
04E9B3  mul edx ; add edi,eax    ; dst += y*pitch
04E9B7  add edi,ecx              ; dst += x
04E9BB  mov ax, word_627B4       ; 宽度来自 **流** 的头
04E9C2  sub edx, eax             ; 行跨度 = pitch - 流宽
```

即 **目的地偏移 = `y*pitch + x`（用传进来的 x/y），而解码尺寸 w/h 来自 RLE 流自己的头**
（`0x2EB9F` 传的 x/y 是子图头里的 w/h，两者常常不等于流宽高：实测 69×61 vs 181×75）。
所以目标必须给**整个画面大小**，不是 `h` 行。已记 `PITFALLS` §8-70。

## 52.4 判据

| 判据 | 结果 |
|---|---|
| `leafcheck` | **72008 cases, 0 failures** |
| `rescheck` / `utilcheck` / `gfxcheck` | 160/0 · 2200/0 · 1450/0（模块各自仍过） |
| `regress.ps1` | **8/8 PASS**，`FD2.TMP=207360` |
| `repl: installed` | **65**（mask 0x1FFF） |
| A/B 同 tick `--replace=none↔all` | **0 / 64000 px** |
| Linux（新分级：构建+自检） | `make` 0 warning、`letest` exact match、`doscheck` 49/49 |
| `translation_map` / `func_ranking` | **65 wired / 1359（4.8%）** |

## 52.5 下一步

按 `re/func_ranking.csv`（重跑后）继续：下一档是
`0x135DD`(98)/`0x11CAC`(84)/`0x26996`(42)/`0x187D6`(41)/`0x126F7`(37)/`0x11DF2`(34)——
它们**依赖更多**（`0x11CAC` 要 5 个未转译函数），建议先把依赖的叶子再收一层，或按
`0x1366A`(110, 818 B) 之前把 `0x11CAC` 的依赖（`0x1297D/0x11EEE/0x122DC/0x127A9/0x1ACF3`）逐个转掉。
