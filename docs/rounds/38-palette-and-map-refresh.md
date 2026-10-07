# 第 38 轮：调色板动画 + 地图视图刷新（§68，第 130–133 个）

> 轮次入口：`build/agents/next_translation.md`（规划 agent 的分配）。
> 真源：`E:\FD2\FD2.EXE.i64`（ida MCP）、`src/repl.c`、`re/func_ranking.csv`。
> 判据：`src/mapcheck.c` 整块缓冲/事件序列逐字节 + `regress.ps1` 8/8 + 同 tick A/B 0 px +
> Linux `letest` exact match / `doscheck` 49/49。

---

## 68.1 目标与结论（一句话）

把 `0x11CAC`（地图视图刷新入口，usage 84）的**唯一剩余依赖**及其同簇两个函数一次转成 C，
并让全表 usage 前二（`0x135DD`=98 / `0x1366A`=110）的依赖闭合：

| addr | size | C 名 | 模块 | REPL 组 | 一句话 |
|---|---|---|---|---|---|
| `0x4E310` | 12 | `pal_tick_word` | `game/fade.c` | `fade` | 读 BIOS tick 字（BDA `0x46C`，走低内存镜像），零扩展 |
| `0x4E31C` | 101 | `pal_anim_step` | `game/fade.c` | `fade` | 每 ≥2 tick 把 `0x60003` 起的 48 字节按帧上传到 DAC `0xE0..0xEF` |
| `0x32230` | 235 | `map_unit_ping` | `game/map.c` | `map` | 按记录 `+32` 查 29 字节表，按调用相位 `%6/%4/%9` 播一次音效 |
| `0x11CAC` | 148 | `map_view_update` | `game/map.c` | `map` | 帧动画 +（`flag==0` 时）调色板动画 + 整条地图视图合成 + 推回 VGA |

**496 B / 4 个函数 / 直接到达点 104 个**。全部只依赖**已接入**的叶子
（`map.c`/`anim.c`/`dlg.c`/`rec.c`/`svc.c`/`gfx.c`/`fade.c`），**未闭合依赖 = 0**。
唯一外部是编译器的 `_chkstk` 栈探针（`PITFALLS` §8-75），零浮点、零 CRT 堆/stdio/文件 I/O。

- `mapcheck`：**122500 cases / 0 failures**（在原 112000 之外新增 10500）。
- 故障注入 4 次全部当场抓到（见 §68.4）。
- `regress.ps1`：**8/8 PASS**、`repl: installed 129 → 133`、`FD2.TMP=207360`。
- 同 tick A/B（`--shot-tick=500` + 固定 autokey）：`none↔none2` **0/64000 px**、
  `none↔all` **0/64000 px**。
- 分组开关：`--replace=fade` → `installed 6`（旧 4 + 新增 2）、
  `--replace=map` → `installed 11`（旧 9 + 新增 2）。
- Linux：`make -f Makefile.linux` 通过（**0 warning**）、`letest` 三对象 **exact match**、
  `doscheck` **49/49**。
- `translation_map`：**133 / 1359（9.8%）**（`--check` 通过）。

**闭环价值**：转完后 `0x135DD`(98)、`0x1366A`(110)、`0x196CB`、`0x11AA8`、
`0x11B48/9B/BFA/C59` 的依赖全部闭合；`0x205DA` 只剩 `0x1088D`，`0x1300D` 只剩
`0x13460`+`0x13A44`（都是已闭合小叶子）。即全表 usage 最高的两个未转译函数已推到
"下一轮可整体转"。

---

## 68.2 反编译语义（以机器码为准）

四个函数都是 **cdecl 栈参、调用者清栈**；入口的 `push <帧>; call 0x3702F` 是 Watcom
`_chkstk` 栈探针伪像（`PITFALLS` §8-75）。先用 `ida_typeinf.apply_tinfo` 打真原型再
`ida_hexrays.decompile`（`PITFALLS` §8-80）。

### 68.2.1 `0x4E310` — `pal_tick_word`（`uint16_t f(void)`）

```
push esi; xor eax,eax; mov esi,46Ch; lodsw; pop esi; retn
```

`xor eax,eax` + `lodsw` ⇒ **零扩展**（不是 `movsx`）。`0x46C` 立即数在宿主/check 里被
`dos_patch_lowmem_refs`/`patch_lowmem_refs` 改到低内存镜像；C 走 `DOS_LOWMEM_BASE + 0x46C`。

### 68.2.2 `0x4E31C` — `pal_anim_step`（`void f(void)`）

```c
if ((uint16_t)(pal_tick_word() - word_60000) >= 2u) {
    if (++byte_60002 == 16) byte_60002 = 0;
    p = 0x60003 + 3 * byte_60002;
    for (i = 0, idx = 0xE0; i < 16; i++, idx++) {
        outp(0x3C8, idx); outp(0x3C9, *p++);
        outp(0x3C9, *p++); outp(0x3C9, *p++);
    }
    word_60000 = pal_tick_word();          /* 写完整段 DAC 后再读一次 */
}
```

- 判据是 16 位差值 `(uint16_t)(tick - word_60000) >= 2`（`sub ax` / `cmp ax,2` / `jb`），
  含 `0xFFFF -> 0x0000` 回绕；`word_60000` 存的是**上传后**再读的 tick。
- `byte_60002` 是 `uint8`；数据指针**不复位**，帧 f 读 `0x60003 + 3*f` 起 48 字节，
  f>0 时会读到 `0x60003+48` 之后——**照机器码照读，不要"修正"**。
- **DAC 写是内联 `out dx,al`（opcode `EE`），不经过 CRT `outp`**：ring 3 会 `#GP`，
  check 侧必须用 VEH 接住（§68.3.1）。宿主里 `dos.c` 的 `emulate_priv_instr` 一直这么干。

### 68.2.3 `0x32230` — `map_unit_ping`（`void f(int idx)`）

机器码把 29 字节表 `*(0x52725)` 拷到栈上，再 `t[k-1]`（`k = *(u8*)(record+80*idx+32)`）：

```c
memcpy(t, 0x52725, 29);
if (rec_skip(idx)) {                            /* 0x1F183 */
    if ((uint8_t)byte_54132 % 6 == 0) sfx(bank, 10, 1);
} else {
    s = t[k - 1];                               /* k 是 1..29 的 1-based 索引 */
    if (s == 0)      { if ((uint8_t)byte_54132 % 6 == 0) sfx(bank, 9, 1); }
    else if (s == 1) { if ((uint8_t)byte_54132 % 4 == 0) sfx(bank, 9, 1); }
    else             { if ((uint8_t)byte_54132 % 9 == 0) sfx(bank, 11, 1); }
}
++byte_54132;
```

- `byte_54132` 是 uint8 自由运行计数器（`movzx` 后 `idiv` 的余数），环绕 256。
- 音效走 `sub_25A96`（`svc_play_sfx`），参数 `(bank=*(0x53EEC), index, loops=1)`：
  C **必须经 `0x25A96` 地址调用**（不是 C 符号），否则 check 两侧不可观测且会撞未初始化的 AIL。
- **`k` 的有效域是 1..29**：`0x52725` 只有 29 字节（全 exe 仅此一处引用），
  越界 `k` 在原文里读的是 29 字节**栈副本**之外的栈垃圾，不可复现（`PITFALLS` §8-84）。
  游戏数据保证 `record+32 ∈ 1..29`。
- 5 个调用点全部立刻覆盖 EAX ⇒ C 返回 `void`（`mapcheck` 只比对音效事件 + `byte_54132` + 记录表不变）。

### 68.2.4 `0x11CAC` — `map_view_update`（`void f(int flag)`）

```c
anim_frame_step();                                      /* 0x1297D */
if (flag == 0) pal_anim_step();                         /* 0x4E31C */
view = dword_53A49 + 0x8088;                            /* 32904 */
map_render_view(view, 456, 13, 8, dword_53AA9, dword_53AAD);   /* ox,oy */
map_reveal_cursor();                                    /* 0x122DC */
dlg_portraits_refresh();                                /* 0x127A9 */
map_draw_cursor(view, 456);                             /* 0x1ACF3 */
gfx_copy_rows((void *)0xA0504, 320, view, 456, 312, 192);      /* 0x11EB0 */
```

`flag` 只判 `==0`；`ox`/`oy` 就是 `0x53AA9`/`0x53AAD`。本簇内被调函数**直接调 C**
（`anim_frame_step`/`map_render_view`/… 都已接入且逐字节对拍过），**不写 `ORIG_*`**——
沿用 `map.c` §67.3 的约定。

---

## 68.3 对拍 harness（扩 `src/mapcheck.c`）

### 68.3.1 唯一的新工程件：窄 VEH `out` 陷阱

C 侧 `pal_anim_step` 走 `OUTP`（`0x37AE5`，被 hook 成 `stub_outp`），
**机器码侧走内联 `out dx,al`**，两者汇进同一条 `(port,value)` 事件日志：

```c
static LONG CALLBACK dac_veh(EXCEPTION_POINTERS *ep) {
    CONTEXT *c = ep->ContextRecord;
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    if (c->Eip < 0x4E31C || c->Eip >= 0x4E31C + 101)
        return EXCEPTION_CONTINUE_SEARCH;      /* 其它特权指令保持原样报错 */
    p = (uint8_t *)c->Eip;
    if (p[0] != 0xEE) return EXCEPTION_CONTINUE_SEARCH;   /* 只认 out dx,al */
    dac_rec(c->Edx & 0xFFFF, c->Eax & 0xFF);
    c->Eip += 1;                               /* 1 字节指令 */
    return EXCEPTION_CONTINUE_EXECUTION;
}
```

只复制 `dos.c` DAC 那一条路径，**不链接整个 `dos.c`**。`stub_outp` 与 VEH 写同一数组，
比较时不分来源。

### 68.3.2 四段用例（原机器码先跑 → C 再跑 → 逐字节/逐事件比较）

| 目标 | 合成输入 | 判据 | 用例 |
|---|---|---|---|
| `pal_tick_word` | 受控镜像 BDA tick | 返回值相等 | 500 |
| `pal_anim_step` | `word_60000∈{t,t-1,t-2-rnd,t-0x8000}`、`byte_60002∈0..15`、`0x60003` 起 128 B 随机 | 完整 DAC 事件序列（含未生效时序列为空）+ `word_60000` + `byte_60002` | 3000 |
| `map_unit_ping` | `idx` 合法、`+32∈1..29`、`rec_skip` 两条路径受控、`byte_54132` 全 0..255、`0x52725` 原样 | 音效事件（bank 哨兵/index/loops）+ `byte_54132` + 记录表未变 | 3000 |
| `map_view_update` | 复用地图视图环境（cell 表/tileset/`53A5D`/`53A61`/`53A81`/`53AFF`/`53B03`），`flag∈{0,1,-1}`、ox/oy 在 32×32 视图内 | `bitmap+0x8088` 起整块 400 KB + VGA `0xA0000` 64 KB + `scr`/`exp`/cell 表/记录表 + 13 个 int32 相位全局 + DAC 事件 | 4000 |

合计新增 **10500**（旧 112000 → **122500**）。`map_view_update` 那段每次跑两遍
`setup_view()`（同 `g_rnd` 种子）保证机器码侧与 C 侧从同一内存状态出发。

`dac_veh` 自检：全部跑完后若 `g_veh_n == 0`，说明机器码侧 0x4E31C 根本没被 VEH 接住，
harness 自身失败（防止"静默不再比原机器码"）。

### 68.3.3 故障注入（harness 自检，4/4 当场抓到）

1. `pal_anim_step` 的 `>= 2u` 改成 `>= 1u` → `FAIL pal_anim_step`，dac `0/64`、`frame 10/11`。
2. `pal_anim_step` 起始 DAC 索引 `0xE0` 改成 `0xE1` → `FAIL pal_anim_step`，dac `64/64` 但值不同。
3. `map_unit_ping` 的 class-0 `%6` 改成 `%5` → `FAIL map_unit_ping`，`sfx 0/1`。
4. `map_view_update` 的 `flag == 0` 改成 `flag != 0` → `FAIL map_view_update`，dac `0/64`。

### 68.3.4 稳定性

`mapcheck` **16 连跑全 PASS `122500 cases, 0 failures`、exit 0**（连续 streak=16）。
本轮修掉了新段暴露的两个环境坑，见 §68.5。

---

## 68.4 接入与分组

`src/repl.c` 只加 4 行、**不新增 `REPL_*` 位**：

| addr | 名字 | group |
|---|---|---|
| `0x4E310` | `pal_tick_word` | `REPL_FADE` |
| `0x4E31C` | `pal_anim_step` | `REPL_FADE` |
| `0x32230` | `map_unit_ping` | `REPL_MAP` |
| `0x11CAC` | `map_view_update` | `REPL_MAP` |

`--replace=all` **129 → 133**；`fade` 4→6、`map` 9→11。分组互不依赖：C `pal_anim_step`
直接调 C `pal_tick_word`/`OUTP`；C `map_view_update` 直接调 C `pal_anim_step`。
`svc.c`/`dlg.c` 里指 `0x4E310`/`0x4E31C` 的 `ORIG_TICK`/`ORIG_PALETTE` 在宿主里被
`repl.c` 改写成指向同一份 C，行为不变（对拍 harness 各自仍用原机器码）。

`build.ps1` 的连带改动：`map.c` 现在引用 `anim_frame_step`/`pal_anim_step`，
所有链接 `game\map.c` 的 harness（`dlgcheck`/`boxcheck`/`keycheck`/`leafcheck`/`typecheck`）
补上 `game\anim.c` + `game\fade.c`；`mapcheck` 补 `game\fade.c`。`Makefile.linux` 早已含三者。

**顺带修了 `tools/translation_map.py` 的模块归属 bug**：它原来的 `file_hint`（从 section 注释
`(src/game/X.c)` 提取）会跨 section 泄漏，导致第 37 轮起所有 `map.c` 行被错标成
`src/game/fade.c`/`fadecheck`（`0x126F7`…`0x1ACF3` 共 37 行），本轮新行跟着错。改成
**优先在 `src/game/*.c` 里找“行首定义”（函数调用是缩进的）**，找不到才回退到 hint；
并补 `map.c`/`anim.c`/`fx.c` 的 `MODULE_INFO` 和四个新地址的 `CASE_OVERRIDE`。
修后 CSV **0 行 `?`**，第 37 轮的 map 行也一并归位（属数据纠正，不改任何运行时代码）。

---

## 68.5 踩坑与修法（另见 `PITFALLS` §8-83/§8-84）

1. **`0xA0000` VGA 块不是"关键块"**：`le_reserve_address_space_early()` 逐 64 KiB 提交
   `0x10000..0x100000`，只有 `<0x70000` 的块失败才算致命；`0xA0000` 块提交失败时静默继续，
   于是 `memset((void*)0xA0000, …)` 间歇性 `0xC0000005`（`eip` 落在 ucrt `memset`、
   `addr=0xA0000`，约 2%）。修法：`mapcheck` 在 reserve 后**显式**
   `plat_commit(0xA0000, 64 KiB, RWX)`，失败则打印 `VGA block 0xA0000 unavailable` 并
   **干净退出 2**，绝不崩。
2. **`t[k-1]` 的域外值不可对拍**：原文先 `memcpy` 29 字节到栈再索引，`k=0`/`k=255` 读的是
   栈垃圾；C 的 `t[-1]`/`t[254]` 是各自栈布局，永远对不上。harness 把 `record+32`
   限在游戏真实域 **1..29**（`PITFALLS` §8-84）。
3. **`map_view_update` 的默认 53C0B 不能是 4**：`dlg_portrait_draw` 的图标库
   `dword_53A61` 只有 48 个子图，`mode = dword_53C0B`（frame==0 时）若为 4，
   索引 `mode+12*p[2]+3*dir` 会到 49，读到偏移表外 → AV。`anim_frame_step` 正常只产生 0..3。
   harness 的 `setup_view` 用 `rnd()%4`（既有 `dlg_portrait_draw` 段同样如此）。

> 附注：`fadecheck` 100 连跑也有约 2% 的 `reserve failed`（几乎不分配堆的它同样如此），
> 说明这种"宿主映像/DLL 的 ASLR 落进低 1 MiB"是环境噪声，与本轮改动无关；本轮把它
> 从"崩溃"降级为"干净退出 2"。

---

## 68.6 备注 / 边界

1. 本轮只为 `0x135DD`/`0x1366A`/`0x196CB`/`0x205DA`/`0x1300D` 清依赖，
   **不转它们本体**（内部还有 VGA 模式/片头/滚动特例，各自成块）。
2. `0x13460`/`0x13A44`/`0x1088D`、`funcs_1199C` 剩余 34 项、`0x10B4E` 文件/stdio 簇、
   `0x2C67D`（fx `cos/sin`）、`0x4E98D` RLE 大件均**本簇之外，不动**。
3. 不改地址空间布局 / 全局偏移（`docs/HOST-DESIGN.md` §4.1）；新代码只读写真地址。
4. `map_unit_ping` 里**不直接调 C 符号 `svc_play_sfx`**——必须经 `0x25A96` 地址。
5. `0x4E31C` **不写内联汇编/`__outbyte`**——沿用 `fade.c` 的 `OUTP(0x37AE5)`。
6. **不为 VEH 链接整个 `dos.c`**——只复制 DAC 那一条 `out dx,al` 路径。
