# 第 37 轮：地图视图渲染核（§67，第 126–129 个）

> 轮次入口：`build/agents/next_translation.md`（规划 agent 的分配）。
> 真源：`E:\FD2\FD2.EXE.i64`（ida MCP）、`src/repl.c`、`re/func_ranking.csv`。
> 判据：`src/mapcheck.c` 整块缓冲逐字节 + `regress.ps1` 8/8 + 同 tick A/B 0 px +
> Linux `letest` exact match / `doscheck` 49/49。

---

## 67.1 目标与结论（一句话）

把 §66 点名的"场景渲染核"里**依赖已全部闭合的 4 个函数**一次性转成 C，并入既有
`src/game/map.c`（`REPL_MAP` 组，不新增 `REPL_*`、不动其他模块）：

| addr | size | C 名 | 一句话 |
|---|---|---|---|
| `0x11EEE` | 885 | `map_render_view` | 按视图模式把 `w×h` 个 24×24 地图格画进 `dst`（含片头/滚动/闪烁特例） |
| `0x24D22` | 208 | `map_scroll_lines` | 把屏幕缓冲 `dword_53AFF` 整体下移 `n` 行（`n!=0` 只锁存行数） |
| `0x122DC` | 1051 | `map_reveal_cursor` | 按半径 `dword_51A83` 对光标周围做 `map_blit_tile` 揭示（case 6 清可见位） |
| `0x1ACF3` | 446 | `map_draw_cursor` | 叠画选择框/头像 + 数字，并给选中记录画血条 |

**2589 B / 4 个函数 / 37 个到达点**。全部只依赖**已接入**的叶子（`map.c`/`sprite24.c`/`gfx.c`/
`rle.c`/`dlg.c`/`rec.c`），**未闭合依赖 = 0**（唯一外部是编译器的 `_chkstk` 栈探针）。

- `mapcheck`：**112000 cases / 0 failures**（在原 97000 之外新增 15000）。
- 故障注入 4 次全部当场抓到（见 §67.4）。
- `regress.ps1`：**8/8 PASS**、`repl: installed 125 → 129`、`FD2.TMP=207360`。
- 同 tick A/B（`--shot-tick=500` + 固定 autokey）：`none↔none2` **0/64000 px**、
  `none↔all` **0/64000 px**。
- 分组开关：`--replace=map` 单独开 → `installed 9`（`mask 0x2000`，旧 5 + 新增 4）。
- Linux：`make -f Makefile.linux` 通过（**0 warning**）、`letest` 三对象 **exact match**、
  `doscheck` **49/49**。
- `translation_map`：**129 / 1359（9.5%）**（`--check` 通过）。

---

## 67.2 反编译语义（以机器码为准）

### 67.2.1 真 ABI：cdecl 栈参

四个入口都是 `push <帧>; call 0x3702F`（Watcom `_chkstk`），Hex-Rays 的 `__fastcall`/
`__usercall` 是伪像（`PITFALLS` §8-75）。**先把 `ida_typeinf.apply_tinfo` 打成正确原型再反编译**，
Hex-Rays 立刻给出干净签名（本轮关键手法，见 §67.6）：

- `0x11EEE`：`void(uint8_t *dst, int pitch, int w, int h, int ox, int oy)`（6 栈参，
  调用点 `push oy; push ox; push h; push w; push 456; push dst; call; add esp,24h`）。
- `0x24D22`：`void(int n)`（1 栈参）。
- `0x122DC`：`void(void)`（0 参，两处调用都是裸 `call`）。
- `0x1ACF3`：`void(uint8_t *dst, int pitch)`（2 栈参，`push 1C8h; push dst`）。

`0x11EEE` 与 `0x1ACF3` 都以 `JUMPOUT(0x10B46/0x1317D)` 收尾——那是**共享尾声**，
C 写 `return;`（禁止跳机器码）。所有调用点都不读 EAX，全部 `void`。

### 67.2.2 `0x11EEE` 的分支

- 每 BIOS tick（`(int16)BDA[0x46C]`，走低内存镜像 `DOS_LOWMEM_BASE+0x46C`）与
  `dword_53A00` 不同时**翻转 `dword_53A40` 的最低位**（机器码是 `xor byte ptr,1`；
  `dword ^= 1` 只动 bit0，等价）。
- `switch(dword_53C03)`：
  - `{9,24,25,28,29}`：tick 变化时 `gfx_expand_scanlines(dword_53AFF, dword_53B03,
    dword_539FC)` 并推进 16 相位；再 `gfx_copy_rows(dst', pitch, dword_53B03, 320, 312, 192)`。
  - `{17,21,22,27}`：`v = (17||27)?462:408`，
    `gfx_copy_rows(dst', pitch, 53AFF + 3*ox + 2*v*oy + 53B07/2 + v*(53B0B/3), v, 312, 192)`。
  - `23`：tick 变化时 `map_scroll_lines(0)`；再 `gfx_copy_rows(dst', pitch, 53AFF, 312,312,192)`。
  - default：不拷贝。
  - 其中 `dst' = dst + 456*dword_53AF1 + dword_53AED + dword_53AF5`（**所有分支共用**；
    456 = 24 行 × 456 步长）。
- 光标相位：`dword_51A93 == -1` 时按 tick（间隔 >2）每 20 帧推进 `dword_53C1F`；
  否则 `dword_53C1F = dword_51A93`。调色板 = `dword_53A6D + *(u32*)(dword_53A6D +
  4*byte_51A97[dword_53C1F] + 6)`。
- 主循环 `for(row<h) for(i<w)`：cell = `dword_53A51 + 4*(ox + dword_53AC1*(oy+row)) + 4`，
  tile = `*(u16*)cell & 0x3FF`，flags = `*(u8*)(dword_53A69 + 4*tile)`；
  按 **if/else 链** `flags&8 → tile += 2*dword_53A40`、`flags&0x10 → tile += dword_53C0B/2`、
  `flags&4 → tile += dword_53A40`（不是三个独立 if）。sprite =
  `dword_53A5D + *(u32*)(dword_53A5D + 4*tile + 6)`；`cell[3]==0xFF` 走 `sprite24_plain`，
  否则 `sprite24_pal_recolor(spr, d, pitch, pal)`。步进 `cell+=4`、`d+=24`，
  行首 `d = dst + 24*pitch*row`。

### 67.2.3 `0x24D22` 的滚动

```
if (n) { byte_51A10 = (u8)n; return; }
lines = byte_51A10;
tmp = malloc(312*lines);
memmove(tmp, 53AFF + 312*(192-lines), 312*lines);
for (i = 191-lines; i >= 0; --i) memmove(53AFF + 312*lines + 312*i, 53AFF + 312*i, 0x138);
memmove(53AFF, tmp, 312*lines);
free(tmp);
```

`0x138 = 312`，`lines` 是**无符号字节**（`movzx byte_51A10`），所以 `n` 的高位被截断。
堆走 `src/game/guest_mem.h` 的 `guest_malloc/guest_free`（宿主=游戏堆；check=被重定向的
host libc），与 `res.c` 同一约定。

### 67.2.4 `0x122DC` 的半径表

`dword_53AB1`（低 32 位 = x，`+4` = y）是光标坐标；`dword_53AC1` 是地图宽。**先给 `0x126F7`
打上 `(int x, int y, int index)` 原型再反编译**，Hex-Rays 就把 41 个逻辑调用点排得整整齐齐：

- mode 1/2：中心索引 0/1。
- mode 3：中心 14 + 上下左右（2/3/4/5）。
- mode 4：半径 2 菱形共 13 格（索引 1..13）。
- mode 5：半径 3 共 21 格（索引 1..13 + 对角 15..18）。
- mode 6：`*(u8*)(dword_53A51 + 4*(x + dword_53AC1*y) + 7) = 0;`
- default：不画。

`qword_53AB1` 的 64 位算术（`HIDWORD-1` 等）就是 y±k，C 直接写成 `y-1` 等。

### 67.2.5 `0x1ACF3` 的合成

```
if (!byte_51AAB || !byte_51AAC) return;
if (53ABD<=5 || 53AB9>=3) { if (53ABD>5 && 53AB9>9) dword_51A0C=1; }
else dword_51A0C = 242;
p = dst + 157*pitch + dword_51A0C;
rle_decode(53A81 + *(u32*)(53A81+526), 0,0, p, pitch, -1);   /* 子图 130 */
map_cell_info(x, y, info);
sprite24_plain(53A5D + *(u32*)(53A5D + 4*info_tile + 6), p + 5*pitch + 6, pitch);
dlg_draw_number_signed(p + 8*pitch + 43, pitch, dword_51A12[info[5]]);
dlg_draw_number_signed(p + 19*pitch + 43, pitch, dword_51A2A[info[5]]);
rec = dlg_portrait_find();
if (rec != -1 && rec[7]!=121 && !(rec[31]==10 && rec[6]==1)) {
    v = (dword_53C0B==3) ? 1 : dword_53C0B;
    sprite24_plain(53A61 + *(u32*)(53A61 + 4*(12*rec[2]+v)), p + 5*pitch + 6, pitch);
    dlg_draw_number_pair(p + 21*pitch + 9, pitch, *(u16*)(rec+64), *(u16*)(rec+66), 3);
}
```

`info` 是 `0x12E38` 的 8 字节输出（tile u16 / flags u16 / 4 个表字节）；两次数字分别用
`info[5]` 索 `0x51A12` / `0x51A2A`。

---

## 67.3 实现（`src/game/map.c` / `map.h`）

- 全局**留在原地址**用宏读写（`dword_53C03`/`dword_53A40`/`dword_539FC`/`dword_53C1F`/
  `dword_53AB1`/`dword_53AB5` …；它们被几十个未替换点共享，C 必须读写真字）。
- 服务**走已接入的 C**（直接调 `gfx_copy_rows`/`gfx_expand_scanlines`/`sprite24_plain`/
  `sprite24_pal_recolor`/`map_blit_tile`/`map_cell_info`/`rle_decode`/`dlg_draw_number_*`/
  `dlg_portrait_find`）——本簇无 `ORIG_*` 机器码回调。
- 堆用 `guest_malloc/guest_free`；tick 走 `BDA_W(0x46C)`（`../dos.h`）。
- `map.h` 追加 4 个原型；顺手把原来落在 `#endif` 之后的三个声明挪进头文件卫哨内
  （原先 `map_cell_info` 等三行在卫哨外，虽无害但不规范）。
- `src/repl.c`：4 行条目进 `REPL_MAP`；`build.ps1`/`Makefile.linux` 源表本来就有 `game/map.c`，
  无需改。

---

## 67.4 对拍 harness（`src/mapcheck.c` → target `mapcheck`）

新增 4 段（全部"原机器码先跑 → C 再跑 → 逐字节比较"，并把可写的全局/缓冲各给一份副本）：

| 目标 | 合成输入 | 判据 | 新增用例 |
|---|---|---|---|
| `map_scroll_lines` | `byte_51A10`、`n∈{0,随机}`、312×192 缓冲两份 | 整块屏缓冲逐字节 + `byte_51A10` | 3000 |
| `map_render_view` | 12 种 mode × `pitch∈{320,456}` × 合法窗口 × 随机 cell/flag/相位 × tick 镜像 | `dst` + `53AFF` + `53B03` 三块逐字节 **+ 6 个相位/翻转全局** | 4000 |
| `map_reveal_cursor` | `mode∈0..7` × 光标含四角/越界 × 随机 cell | 位图 + 16 KB cell 表逐字节 | 4000 |
| `map_draw_cursor` | 开关位 × `53ABD/53AB9/51A0C` 边界 × 记录表 `+2/+6/+7/+31/+64/+66` 受控 | `dst` 整块 + 记录表未变 | 4000 |

（合计新增 **15000** = 500 轮 × 6/8/8/8；另有 4 个非-map 模块的 harness 各补一行 `dos_lowmem_base`
定义，因为 `map.c` 现在通过 `DOS_LOWMEM_BASE` 读 tick。）

基础设施：把 Watcom CRT 的 `malloc`(`0x3706E`)/`memmove`(`0x3771C`)/`free`(`0x3776E`)
重定向到 host libc（照 `rescheck` 方法二），并把 tileset 子图数从 4 扩到 32
（`map_reveal_cursor` 用到索引 14..18）。新增 `scrA/scrB`(256 KB)、`expA/expB`(128 KB)、
`cellA/cellB`、`recA/recB` 缓冲。

**故障注入（harness 自检，4/4 当场抓到）**：

1. `map_render_view` 的 `flags&0x10` 步进 `dword_53C0B/2` → `/3`：`FAIL map_render_view`，
   case 24 第 201 个用例失败。
2. `map_scroll_lines` 的 `memmove(...,0x138)` → `0x137`：`FAIL map_scroll_lines`，
   屏缓冲字节 58967 起对不上。
3. `map_reveal_cursor` mode 5 少画 `(x+1,y+1,18)`：`FAIL`，mode=5 x=22 y=22。
4. `map_draw_cursor` 的 `v==3→1` 特例改成 `v==2`：`FAIL`，位图 @74010 起不同。

---

## 67.5 验收数据

| 判据 | 结果 |
|---|---|
| `mapcheck` | **112000 cases / 0 failures**（旧 97000 + 新 15000） |
| 故障注入 | 4/4 被抓（见 §67.4） |
| `regress.ps1` | **8/8 PASS**、`FD2.TMP=207360`、`repl: installed 129` |
| `--replace=map` | `installed 9`（`mask 0x2000`；旧 5 + 新增 4） |
| A/B 同 tick `--shot-tick=500` | `none↔none2` **0/64000 px**、`none↔all` **0/64000 px** |
| Linux | 构建 **0 warning**、`letest` 三对象 **exact match**、`doscheck` **49/49** |
| `translation_map` / `func_ranking` | **129 / 1359（9.5%）**（`--check` 通过） |

---

## 67.6 备注 / 边界

1. **本轮的关键手法**：对 `0x11EEE`/`0x122DC`/`0x1ACF3`/`0x24D22`/`0x126F7` 先用
   `ida_typeinf.apply_tinfo` 写入真 cdecl 原型，再 `ida_hexrays.decompile`。未打原型时 Hex-Rays
   把 6 栈参看成 10 个寄存器参数、把 41 个 `map_blit_tile` 调用排成乱序；打完后参数序与
   调用序一目了然。**注意** `ida_typeinf.parse_decl` 在本 IDA 里返回的是 `tinfo_t`（真值），
   不能再写 `if (parse_decl(...)) { 失败 }`（§67 踩坑，`PITFALLS` §8-80）。
2. **未碰** `0x4E31C`（DAC 循环，内联 `out 0x3C8/0x3C9`，check 侧原机器码会 #GP，需先配
   VEH out-trap）——它是 `0x11CAC`/`0x19953` 闭合的**唯一剩余依赖**，下一轮首选项。
3. **未碰**调用者 `0x11CAC`/`0x135DD`/`0x1366A`/`0x196CB`/`0x197E5`/`0x19953`：本轮只清依赖，
   让它们下一轮各自成块。转完本轮后 `0x135DD`/`0x1366A`（全表 usage 前二）**只剩 `0x11CAC`**，
   `0x197E5` 依赖已闭合。
4. **未碰** `0x10B4E` 文件/stdio 簇、`0x2C67D`（fx 浮点）、`funcs_1199C` 剩余 34 项。
5. **A/B 配方**：`build/ab_run.ps1` 的 `WorkingDirectory` 是摆设——宿主 `--gamedir` 默认硬编码
   `E:\FD2`，必须在参数里显式传 `--gamedir=<sandbox>`，否则跑的是真实目录（`PITFALLS` §8-73）。
   本轮用 `--gamedir=E:\FD2\port\build\sandbox` + 固定 autokey + `--shot-tick=500`，基线 0 px。
6. Linux 侧沿用分级：纯游戏逻辑转译只做构建 + `letest`/`doscheck`（无平台/渲染/入口改动）。

---

## 67.7 补记：`mapcheck` 间歇性 0xC0000005（提交后发现，已修）

**症状**：提交 `bb2a440` 后复跑 `./build/mapcheck.exe` 会 **exit 139 / 0xC0000005**，
停在 `redirected 65 low-memory references` 之后、一条 FAIL 都没打印；实测约 1/6 概率
（验收侧连续 3 次全崩）。不是 §48（已过 `reserve failed` 那一步）、不是旧二进制
（从已提交源码重建同样崩）、`regress` 仍 8/8——**只有 `mapcheck` 崩**。

**根因（harness 输入越界，§8-71 同类；机器码侧崩，C 侧无 bug）**：
`map_draw_cursor` 段的"光标不落在任何记录上"分支（`want == n`）喂了哨兵坐标
`dword_53AB1/53AB5 = 0x1234/0x5678`。`map_cell_info`（`0x12E38`，C 版同）**没有边界检查**：

```c
cell = dword_53A51 + 4 * (x + dword_53AC1 * y)   /* = cells + 4*(4660 + 32*22136) ≈ cells + 2.85 MB */
```

`cells` 只有 16 KB ⇒ 越界读 ~2.8 MB；该地址有没有映射取决于 ASLR ⇒ **间歇性**访问冲突。
游戏里光标格永远在地图内，这个哨兵值游戏永远产生不出来。

**定位手法（可复用）**：`rnd()` 是定种子 LCG（`0x5EED1234`）⇒ 每次运行输入逐轮完全相同，
崩点永远在同一轮同一段；在各测试段之间插 `fprintf(stderr, "SEG%02d r=%d\n", …)`（stderr 无缓冲），
崩一次看最后一条即可二分。最终 marker：`DC r=4 i=5 want=2 n=2 x=4660 y=22136 aac=1`
→ 崩在 `ORIG_DCURSOR`（原机器码 0x1ACF3）内、`DC-ORIG-ok` 未打印；同坐标的 i=4 因
`aac=0` 走 early return 没崩——这也解释了为什么时崩时不崩。

**修法**：`src/mapcheck.c` else 分支改为在 32×32 视图里选一个不与任何记录重合的真实格子
（确定性推进，≤ n+1 次必找到），机器码/C 两侧跑同一条"无记录"全路径。

**修后判据**：`mapcheck` **16 连跑全 PASS `112000 cases, 0 failures`、exit 0**
（修前 1/6 崩）；24 个 `*check` 全过；`regress.ps1` 8/8；`translation_map --check` 129 wired。
排查期间的临时插桩已全部删除，提交只含真实修复。
