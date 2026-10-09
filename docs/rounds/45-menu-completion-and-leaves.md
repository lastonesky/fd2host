# 第 45 轮：菜单层收尾 + 语义命名 + 场景/UI 叶子（第 286–297 个）

> 操作者要求“下一批，函数名和文件名尽量有意义，而不是用 1234 代号命名”。
> 本轮先**把第 44 轮的 `ev7_205DA` 式命名整体改成语义名**，再补齐菜单 B 剩余 6 个 +
> 6 个依赖闭合的场景/UI 叶子，共 **12 个新函数**，接入 285 → **297 / 1359（21.9%）**。

## 45.1 模块改名（语义命名）

`src/game/ev7.c/.h` → **`src/game/menu_actions.c/.h`**，分组 `REPL_EV7` → **`REPL_MENU`**，
30 个函数全部改成描述性名字（repl 表 + `ev2check` 对拍名同步）：

- `0x205DA menu_reload_world`
- 菜单 A 拒绝检查 13 个：`menu_need_records_marked` / `menu_need_flags_clear_50_51` /
  `menu_need_flag_clear_14` / `menu_need_any_clear_15_26` / `menu_need_flag_clear_64` /
  `menu_need_flag_clear_65` / `menu_need_flag_clear_52_no_unit18` / `menu_need_flag_clear_64_late` /
  `menu_need_any_clear_26_43` / `menu_need_flags_clear_16_17` / `menu_need_flag_clear_1` /
  `menu_need_flag_clear_16` / `menu_need_flags_clear_1_2`
- 菜单 B 画面动作：`menu_show_reload` / `menu_show_reload_pair` / `menu_show_mark_two` /
  `menu_show_plain`（= `0x3346B`/`0x335A0`/`0x33674` 三个同体入口）/ `menu_show_step20` /
  `menu_show_or_rebuild_sprites` / `menu_show_step16_then_clear` / `menu_show_step9_then_clear`

## 45.2 新增 12 个

### 菜单 B 剩余 6 个（并入 `menu_actions.c`）

| 地址 | 名字 | 作用 |
|---|---|---|
| `0x33169` | `menu_rebuild_sprites_and_show` | 重载 → 画子流 0 → 重建单位精灵 → 两次 stepper → 画子流 1 |
| `0x3327D` | `menu_mark_records_and_show` | 重载 → 11 条记录 `+3=2` → 画子流 0/1 → 清头像 |
| `0x33367` | `menu_rebuild_sprites_and_clear` | 重载 → 画子流 0 → 重建精灵 → 画子流 1 → 步进后画子流 2 + 清 |
| `0x333F5` | `menu_reset_and_show` | 重载 → 复位头像 → 重建精灵 → 两次 stepper → 清 → 画子流 0 |
| `0x334D9` | `menu_show_gated_by_unit` | 按 `unit_exists(12)` 选子流 `3/0`，画三条子流后 glide |
| `0x335DA` | `menu_step_pair_then_clear` | 重载 → 画子流 0 → 两次 stepper → 画子流 1 → glide |

### 依赖闭合的场景/UI 叶子 6 个（按域放进已有模块）

| 地址 | 名字 | 文件 | 作用 |
|---|---|---|---|
| `0x1C269` | `rec_collect_slot_bits` | `rec.c` | 收集记录 `+26..+30` 五个字节置位的下标（`8*i+b`），返回个数 |
| `0x311E5` | `anim_cycle_frame` | `anim.c` | 按 (帧, 子步) 游标 blit 一帧并前进；`mode==0` 只复位 |
| `0x1E0DB` | `map_enqueue_status` | `map.c` | 把数值的四位数字字形排进状态图标队列（`byte_53C6C/53D34/53DFC`） |
| `0x233C6` | `scene_place_records` | `scene.c` | 淡出 → 按 xs/ys 数组摆放一组记录 → 设视口原点 → 淡入 |
| `0x31BDF` | `msg_show_lines` | `msg.c` | 一页对话：开头像 → 画 vm 子流 → 开框/等键 → 关头像 |
| `0x1E529` | `msg_show_page` | `msg.c` | 画对话页（`page==3` 回落到闭嘴头像），推进计数并返回下一页 |

## 45.3 关键实现点

1. **`loc_3312D` 的子流号由调用者压栈决定**（第 44 轮已记）：`loc_33028` 传 1、`0x33AAE` 传 0；
   本轮又确认 `loc_3310C`（`0x33367`/`0x335DA` 的尾）传的是 **2**。C 里统一成
   `menu_tail_drawN_clear(sub)`。
2. **对拍 harness 的 obj1/obj2 是逐字节比较，指针全局两侧必须同值**：第一版 `leave_args`
   把 `0x53A69`（路径形状表）设成“各自一侧的 rec 缓冲”，于是 obj1 里存的是不同地址，
   报 `obj1+3A6A orig=62 ours=B2`。改成两侧都指向同一块只读 scratch（`rec_o+4500`）后通过。
3. **`scene_place_records` 的 `0x11CAC` 需要参数 1**：C 里写成 `ORIG_VIEW()` 会传进垃圾
   （`view,1601907161`），应为 `ORIG_VIEW(1)`（同一类错在 §75 也犯过一次）。
4. **两个地图叶子本轮不接入**：`0x14818 map_reveal_reachable` 的路径洪泛会写 obj2（对拍里
   两侧分叉）、`0x12CEA map_slide_view` 的地图 stepper 解引用未初始化的 `0x53A49` 直接段错误。
   它们的 C 已写好（`map.c`），等一个“地图世界”对拍 harness 再接入；**未对拍就不 wire**。

## 45.4 判据

```
ev2check --only=<12 个> --cases=20：240/0
ev2check --cases=15 全量：2355/0
regress：all 8/8、none 8/8          repl: installed 297 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：297 wired / 1359（21.9%），0 translated-not-wired
```

## 45.5 下一步

- 菜单 B 还有 4 个更大的 handler（`0x336A0`(548)/`0x338C4`(166)/`0x3396A`(324)/`0x33AF1`(428)），
  收掉后两族闭合。
- 补一个**地图世界对拍 harness**，把 `map_reveal_reachable`/`map_slide_view` 接进来。
- 按 usage：`0x10652`(571)/`0x1088D`(705)（`menu_reload_world` 的两个前置，走 `ev6check`）、
  `0x197E5`(366)/`0x19953`(1188)（场景绘制，`0x1AA1D` 解释器簇缺口）、`0x1DF58`/`0x1C2DA`/`0x1C4CC`
  （堆用户，同批上 CRT 重定向对拍）、`0x2C67D`（`funcs_30469[6]`，含 `cos/sin`）。
