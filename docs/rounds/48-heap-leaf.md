# 第 48 轮：堆叶子第一刀 `res_draw_subimage`（第 303 个）

> 延续第 47 轮的 CRT 重定向对拍器，收堆叶子。本轮落地 **`0x15F0E`**（并把 `worldcheck`
> 扩成能测“分配-绘制-返回记录”型叶子），接入 302 → **303 / 1359（22.3%）**。

## 48.1 新增 1 个：`0x15F0E res_draw_subimage`

原函数（6 个 cdecl 栈参 `(tbl, surface, stride, base, row, index)`）：

```
e   = tbl + *(u32*)(tbl + index*4 + 6);       // 子图偏移表（表项 +6 起）
w/h = e 的 u16 头
off = row*stride + base;
rec = malloc(w*h + 8);                         // 返回给调用者的“背景记录”
gfx_save_rect(rec, w, h, surface, off, stride); // 存背景
rle2_blit_trans(surface + off, e, stride);      // 透明贴子图
return rec;                                     // 尾 0x15983: mov eax,edi
```

放进 `src/game/res.c`（与 `res_load`/`res_blit` 同族），分组 `REPL_RES`。

## 48.2 `worldcheck` 扩展

在已有的“真实文件 + CRT 重定向”harness 上加一个堆叶子组：

- 合成一张子图偏移表 `g_tbl`（8 个表项都指向同一张 4×4 solid-fill 的 RLE 子图）；
- `surface = heap_o`（0x40000 的 `dword_53A49` 缓冲）、每轮随机 `base/row/index`；
- 把 `0x4ECBF→gfx_save_rect`、`0x4EBAB→rle2_blit_trans` 钩到已对拍的 C，
  `0x3790A→no-op`；
- 逐字节比对 **surface 缓冲** 和 **返回的背景记录**。

`--cases=40`（tiles/party 与子图交替）= **40/0**。

## 48.3 仍未接入：另外 3 个堆叶子

`0x1DF58`（状态数字上浮动画）的 C 已按反汇编写好（含 `+0x8088` 与 `456*(v3-3)` 这两处
容易漏的偏移），但**原机器码在本 harness 里仍段错误**（世界差异：它不越界检查记录坐标，
而合成世界必须让所有记录落在视图内并把 shape 表铺好）。`0x1C2DA`/`0x1C4CC` 同类
（画队伍图标 + 音效，带视图边界检查）。这三个等“地图世界 harness”补齐后再接，
**未对拍就不 wire**。

## 48.4 判据

```
worldcheck --cases=40：40/0
regress：all 8/8          repl: installed 303 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：303 wired / 1359（22.3%），0 translated-not-wired
```

## 48.5 下一步

1. 补“地图世界”对拍世界（视图原点 + 记录坐标 + shape 表 + 屏幕缓冲），一次收
   `0x1DF58`/`0x1C2DA`/`0x1C4CC` 与场景绘制 `0x197E5`/`0x19953`/`0x1DB65`、
   `0x14818`/`0x12CEA`。
2. `0x24618` 及其依赖链 `0x22046`/`0x219AD`（堆 + 未转译依赖，同一把 CRT 重定向对拍器）。
3. 逐个补 `0x2C67D`（`funcs_30469[6]`，含 CRT `cos/sin`）等零头。
