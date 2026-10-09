# 第 44 轮：菜单/队伍 action handler 批量（24 个，第 262–285 个）

> 操作者要求“继续逆向（20–30 个一批）”。本轮按 `re/func_ranking.csv` + 调用图，
> 选中**两个菜单分派表的 action 簇**——它们只依赖已接入的 `0x205BE`/`rec_flag`/
> `unit_exists`/`vm_run` 等——共 **24 个**，接入 261 → **285 / 1359（21.0%）**。

## 44.1 批次内容（24 个，新模块 `src/game/menu_actions.c/.h`，新分组 `REPL_MENU`）

### 0x205DA（1 个）

| 原地址 | 大小 | 作用 |
|---|---|---|
| `0x205DA` | 163 | 重载世界：`0x1088D(dword_53C03)` → 清 `0x53AD5` 视口原点/计数器 → `map_view_update(1)` → `pal_fade_in()` → `dword_53BEF=1` → `kbd_flush()` |

### 菜单 A：记录标志校验（13 个）

| 原地址 | 大小 | 作用 |
|---|---|---|
| `0x206C5` | 66 | 记录 5..10 必须都置了 `+5` 的 bit0，否则 `dword_53ECC=1` |
| `0x20707` | 54 | `rec_flag(50) \|\| rec_flag(51)` → 失败 |
| `0x2073D` | 40 | `rec_flag(14)` → 失败 |
| `0x20765` | 189 | “菜单 10”长校验：`rec_flag(15..26)` 全置则失败并画子流 10；`dword_53BEF>5` 且 `rec_flag(59)` 再失败并画子流 2 |
| `0x20822` | 40 | `rec_flag(64)` → 失败 |
| `0x2084A` | 40 | `rec_flag(65)` → 失败 |
| `0x20872` | 93 | 仅当 `unit_exists(18)==0`：`rec_flag(52)` 则画子流 2 并失败 |
| `0x20926` | 49 | `dword_53BEF>6` 且 `rec_flag(64)` → 失败 |
| `0x20957` | 250 | 两段校验（记录 `0x26..0x2D`、`0x2E..0x43` + `rec_flag(0)`/`rec_flag(0x34)`），失败码 1/2 |
| `0x20A51` | 54 | `rec_flag(16) \|\| rec_flag(17)` → 失败 |
| `0x20A87` | 40 | `rec_flag(1)` → 失败 |
| `0x20B14` | 40 | `rec_flag(16)` → 失败 |
| `0x20B3C` | 54 | `rec_flag(1) \|\| rec_flag(2)` → 失败 |

### 菜单 B：脚本/vm action（10 个）

| 原地址 | 大小 | 作用 |
|---|---|---|
| `0x3314B` | 30 | 重载 → `dword_51A83=0` → 共享尾 `vm_run(sub=0)` + `dlg_portrait_glide(0)` |
| `0x33219` | 100 | 重载 → 135DD(7,32)/1366A(31) → 子流 0 → 135DD(7,23)/1366A(32) → 共享尾(子流 1 + `dlg_portrait_clear`) |
| `0x3332B` | 60 | 重载 → 135DD(10,0) → 记录 `+4038`/`+4118`=100 → 共享尾(子流 0) |
| `0x3346B` | 17 | 重载 → 共享尾(子流 0) |
| `0x3347C` | 29 | 重载 → 135DD(20,20) → 共享尾(子流 0) |
| `0x335A0` | 10 | 共享体 `0x33470`：重载 → 共享尾(子流 0) |
| `0x335AA` | 48 | 重载；`unit_exists(18)==0` 时 `unit_sprites_build(1)` → 共享尾(子流 0) |
| `0x33674` | 10 | 共享体 `0x33470` |
| `0x3367E` | 34 | 重载 → 135DD(16,28) → 1366A(67) + `dlg_portrait_clear` + 共享尾(子流 0) |
| `0x33AAE` | 67 | 重载 → 135DD(9,39)/1366A(76) → 共享尾（子流 **0** + `dlg_portrait_clear`） |

## 44.2 关键实现点

1. **IDA 的 `__fastcall/__usercall` 全是假象**。这些函数在机器码里都是 cdecl：
   `push idx; call 0x34894; add esp,4`（见 `0x2073D`/`0x20B3C` 反汇编）。C 侧按
   “最后一个 push = 第一个参数”写即可，`src/game/menu_actions.c` 头部注明。
2. **共享尾块**（同 §8-87）：
   - `loc_3344D → loc_33206 → loc_33140` = `vm_run(stream, 0, …)` + `dlg_portrait_glide(0)`；
   - `loc_3312D` 是同一段但**由调用者压入的 8 号参数（sub）决定子流**：`loc_33028` 传 1、
     `0x33AAE` 传 **0**（第一版误按 1 写，`ev2check` 报 `orig 0 / ours 1` 后修正）；
   - `loc_33440` = `ev4_1366A(sel)` + `dlg_portrait_clear()` 后落进 `loc_3344D`。
   C 里抽成 `menu_tail_d/g/h` 与 `menu_tail_e(sel)`。
3. **`0x33470` 是共享体**：`0x335A0`/`0x33674` 只是 `push 0x28; jmp 0x33470`，C 里与
   `menu_3346B` 同体。
4. **所有外部依赖走固定地址函数指针**（`ORIG_*`）：宿主里 repl 已把它们换成 C，
   对拍 harness 里则被 hook 成桩，两侧对称。`0x205DA` 调 `0x1088D` 也走地址——
   `ev2check` 把 `0x1088D`/`0x12D7B`/`0x1F525` 桩掉，避免真去开文件/写 DAC。

## 44.3 harness

`src/ev2check.c` 扩 batch-7：

- 新增 `#include "game/menu_actions.h"`、24 个 `O_xxxxx` 原入口 + `E7(a)` 宏生成的 `o_/c_` 包装；
- 新桩 `stub_1088d`(EV_EXT 20)、`stub_glide`(21)、`stub_fadein`(22)，
  并 `HOOK(0x1088D/0x12D7B/0x1F525)`；
- 表项追加 24 条，`--only` 子集照常可用。

## 44.4 判据

```
ev2check --cases=20 --only=<24 个>：480/0
ev2check --cases=20 全量（145 项）：2900/0
regress：all 8/8、none 8/8          repl: installed 285 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：none↔none 0/64000、none↔all 0/64000 px
letest：reference check OK, exact match
Linux：make 0 warning、letest-linux 三对象 exact match、doscheck-linux 49/49
translation_map：285 wired / 1359（21.0%），0 translated-not-wired
```

## 44.5 下一步（按 usage）

菜单两族还剩**更大**的同族块（未接入）：`0x33169`(176)/`0x3327D`(174)/`0x33367`(142)/
`0x333F5`(118)/`0x334D9`(199)/`0x335DA`(154)/`0x336A0`(548)/`0x338C4`(166)/`0x3396A`(324)/
`0x33AF1`(428)——`0x333F5`/`0x33169`/`0x335DA` 正好是上面共享尾的**宿主**，一并收掉后
两族闭合。其余按 `re/func_ranking.csv`：`0x10652`(571)/`0x1088D`(705)（`0x205DA` 的两个
前置，走 `ev6check` 的 CRT 重定向对拍）、`0x197E5`(366)/`0x19953`(1188)（场景绘制，
`0x1AA1D` 解释器簇的缺口）、`0x14818`(480)、`0x12CEA`(145)、`0x233C6`(245)、
`0x1E0DB`(257)、`0x1C4CC`(658)、`0x2C67D`(1151，`funcs_30469[6]`，含 CRT `cos/sin`)。
