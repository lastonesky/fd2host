# 第 41 轮：场景移动 / 地图窗口 / 头像关闭（10 个，含 usage #1）

> 批量节奏第 3 批。第 39/40 轮把 `funcs_1199C` 的场景脚本簇收完后，本轮转向
> **全表 usage 最高的一族**：`0x1366A`（usage 110）及其地图/滚动/等待叶子。

## 41.1 批次内容（10 个）

| 原地址 | 大小 | usage | 作用 |
|---|---|---|---|
| `0x1366A` | 818 | 110 | 场景移动动画总驱动（读 `sub_4EB48` 脚本，走/放置/重绘三态） |
| `0x11AA8` | 160 | — | 等键（BIOS INT 16h），等待期间跑调色板/地图动画 |
| `0x11B48` | 83 | — | 地图窗口上移一格 |
| `0x11B9B` | 95 | — | 地图窗口下移一格 |
| `0x11BFA` | 95 | — | 地图窗口右移一格 |
| `0x11C59` | 83 | — | 地图窗口左移一格 |
| `0x12263` | 121 | — | 刷新每格记录计数（`dword_53A51` 4 字节格子） |
| `0x1E1DC` | 182 | — | 记录入光标队列（4 槽，写 `byte_53D34/53DFC/53C6C`） |
| `0x24B4D` | 145 | — | 地图滚动动画帧（`map_render_view`+`gfx_copy_rows`+delay） |
| `0x196CB` | 129 | — | 关闭场景头像（5 条带 blit + VGA 还原 + 释放三缓冲） |

新模块 `src/game/ev4.c/.h`，接入新分组 **`REPL_EV4`（bit 0x20000）**。

**推迟到下一批**（同一大簇的剩余部分）：`0x1AA1D`（脚本解释器，726 B）及其三个前置
`0x197E5`/`0x19953`（头像菜单绘制/交互，366+1188 B）、`0x1DB65`（地图/头像刷新，857 B）——
它们要 malloc + `int386` + 整屏 memmove，本轮先把 harness 的 VGA/低内存/INT16 缝建好
（见 §41.2），下一批直接收。

## 41.2 对拍 harness 扩展（`src/ev2check.c`）

- **VGA 快照**：commit `0xA0000..0xAFFFF` 并纳入比较（`0x196CB` 的
  `memmove(0xA0000, screen, 64000)` 因此可对拍）。
- **低内存重定向**：把 obj0 里 `0x400..0x500` 的绝对立即数改到 `0x70000` 镜像
  （照 `src/leafcheck.c`），`dos_lowmem_base=0x70000`；`0x11AA8` 读 BDA tick `0x46C`。
- **新钩子**：`0x11D40`/`0x11EB0`/`0x11EEE`/`0x127E0`/`0x129EC`/`0x1297D`/`0x127A9`/
  `0x32230`/`0x1974C`/`0x370F0`(INT16)/`0x10620`(等键)/`0x4E31C`(调色板)/
  `0x4EB48`（返回一份**合成脚本**，让 `0x1366A` 三态都跑到）。
- **不再钩** `0x12263`/`0x196CB`/`0x1366A`（它们是本轮被测对象）。
- `dword_53AB9`/`dword_53ABD` 也随机成**完整 32 位**（含负数），见 §8-89。

## 41.3 两个真 bug（一个 harness 抓、一个只有宿主 A/B 抓）

1. **`0x1366A` 两处漏 `wait(1)`**（harness）：走态与置放态各有一个
   `push 1; call 0x17AA9`，我漏了；对拍报
   `event 49 orig wait,1 … ours flush` 与 `orig wait,1 … ours ext,3`。补齐后过。
2. **`0x11B9B` 的有符号 `jle` 写成了无符号比较**（**只有宿主 A/B 抓**）：
   机器码 `cmp dword_53ABD,5; jle` 是**有符号**；C 里 `dword_53ABD` 是 `uint32_t`，
   `<=5` 走无符号。游戏里 `dword_53ABD` 会变负 ⇒ `all` 与 `none` 静态帧差 **22232/64000
   （34.7%）**，而 `ev2check` 全过（合成用例里它是小正数，没覆盖负数）。
   - **定位**：`FD2_REPL_SKIP` 逐半二分 → `0x11B9B`；修成 `(int32_t)dword_53ABD <= 5`
     （`0x11B48/0x11BFA/0x11C59` 同类比较一并修）后 A/B **0/64000**。
   - **harness 补强**：`setup_world` 把 `dword_53AB9/53ABD` 随机成完整 32 位，重跑
     4 个 stepper 各 300 例全过。→ `PITFALLS` §8-89。

## 41.4 判据

```
ev2check --only=…：
  11B48 11B9B 11BFA 11C59 12263          PASS 1000/0
  1E1DC 24B4D 196CB 11AA8 1366A          PASS 1000/0（修两处 wait 后）
  steppers（带负数计数器重跑）           PASS 1200/0
  全量 68 个 --cases=20                   PASS 1360/0
regress：all 8/8、none 8/8                repl: installed 191 → 201
静态帧 A/B --shot-tick=500：0 / 64000 px
Linux：make 0 warning、letest exact match、doscheck 49/49
```

## 41.5 下一步

- **解释器簇收口**：`0x1AA1D` + `0x197E5` + `0x19953` + `0x1DB65`（harness 的 VGA/INT16/
  malloc 缝已就绪；`0x1DB65` 还需 `0x4EBAB`/`0x25A96` 钩子）。
- `0x10B4E`(usage 58, 258 B) `FDICON.B24`+`FDFIELD.DAT` 载入 + `FD2.TMP` 重写
  （需 `0x10C50`(969) + CRT 文件缝）。
- 之后回补 `funcs_1199C` 索引 0..37 未收的 25 个 `0x34xxx` 记录/状态 handler。

记录：`python tools/translation_map.py`（191→**201**/1359，14.8%）。

## 41.6 交付物

`src/game/ev4.c/.h`、`src/ev2check.c`（扩 VGA/低内存/12 钩子）、`repl.c/.h`（`REPL_EV4`）、
`build.ps1`/`Makefile.linux`、`docs/rounds/41-scene-move-and-map.md`（本文）、
`re/ev4_disasm.txt`、`re/ev4_decompile.c`、`re/ev4b_dec.c`。
