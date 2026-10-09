# 第 46 轮：菜单 B 尾块（第 298–300 个）

> 继续菜单层收尾。本轮把菜单 B 里**对拍可控**的 3 个尾块收进 `src/game/menu_actions.c`，
> 接入 297 → **300 / 1359（22.1%）**。

## 46.1 新增 3 个

| 地址 | 名字 | 作用 |
|---|---|---|
| `0x338C4` | `menu_show_step_pairs` | 重载 → 画子流 0 → 重建精灵 → 头像依次走 `(0,4)/(0,22)/(26,24)/(26,2)`，每步停 400 ms → 画子流 1 + glide |
| `0x3396A` | `menu_show_map_pan` | 重载 → `res_load("FDOTHER.DAT",0,88)` 当效果库 → 画子流 1 → 清地图位图 → 4 次“音效 + 地图平移(20/20/20/60)” → 画子流 2 → glide → 停库并释放 |
| `0x1D4F6` | `menu_stop_and_free_music` | `sfx(bank,-1,1)` 停库 → `free(bank)` |

## 46.2 关键实现点

1. **`loc_331EA` 的 `add esp,4` 只是清 cdecl 参数**：`0x338C4` 末尾 `push 400; call delay`
   之后没有 `add esp,4`，而是 `jmp loc_331EA`，由尾块的 `add esp,4` 清掉那个 400。
   C 里按普通 cdecl 写 `ORIG_DELAY(400); menu_vm(1); ORIG_GLIDE(0);` 即可（第一版以为尾块
   跳过 delay，复核字节码 `e8 a5 3f 00 00` = `call delay` 后确认）。
2. **`0x1D4F6` 的尾部 `jmp loc_1A80A` = `call free; retn`**：它先把 `dword_53B13` 压栈当
   `free` 的参数，所以 C 就是 `ORIG_SFX(bank,-1,1); ORIG_FREE(bank);`。
3. **对拍里的地图位图 `0x53A49`**：`0x3396A` 会 `memset(0x53A49, 0, 0x25680)`，所以 harness
   必须给它一个合法且两侧同值的缓冲（`g_mapsrc[160000]`，每次调用前重填），否则段错误/obj1 分叉。

## 46.3 判据

```
ev2check --only=<3 个> --cases=20：60/0
ev2check --cases=15 全量：2400/0
regress：all 8/8          repl: installed 300 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：300 wired / 1359（22.1%），0 translated-not-wired
```

## 46.4 仍未接入的菜单 B 大块（需要新 harness）

- `0x336A0`(548)、`0x33AF1`(428) 都调用 `0x24618`；`0x24618`(316) 用 `malloc(0x25680)` 并且
  调用 `0x22046`，而 `0x22046`(214) 又调用未转译的 `0x219AD`。这一串是**堆 + 未转译依赖**，
  要在对拍里用 CRT 重定向（`0x3706E malloc` → 宿主）先补齐，和 `0x1DF58`/`0x1C2DA`/`0x1C4CC`/
  `0x15F0E`（堆叶子）一起做。
- `0x197E5`(366)/`0x19953`(1188) 场景绘制用大屏幕缓冲 + 地图位图，且 `0x19953` 是**等键循环**
  （`kbd_pending`/`int386`），需要“地图世界 + 确定性键盘”对拍，和 `map_reveal_reachable`/
  `map_slide_view` 一起排。

## 46.5 下一步

1. **CRT 重定向对拍器**：把 `0x3706E/0x3776E`（malloc/free）在 `ev2check`（或新工具）里改成
   宿主 libc，一次收 `0x15F0E`/`0x1DF58`/`0x1C2DA`/`0x1C4CC`/`0x24618`/`0x10652`/`0x1088D`。
2. **地图世界 harness**：收 `0x197E5`/`0x19953`/`0x1DB65`/`0x14818`/`0x12CEA`。
3. 解释器簇 `0x1AA1D`（`0x1B932`/`0x17E0B`/`0x1B9DE` 零头）。
