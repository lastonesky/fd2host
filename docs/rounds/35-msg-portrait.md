# 第 35 轮：对话框/头像合成三件套（§65，第 112–114 个）

> 轮次入口：`build/agents/next_translation.md`（规划 agent 的分配）。
> 真源：`E:\FD2\FD2.EXE.i64`（ida MCP）、`src/repl.c`、`re/func_ranking.csv`。
> 判据：`src/msgcheck.c` 逐字节/整幅/事件序列对拍 + `regress.ps1` 8/8 + 同 tick A/B 0 px
> + Linux `letest` exact match / `doscheck` 49/49。

---

## 65.1 目标与结论（一句话）

把第 28 轮 §58.5 **显式后置**的三个"头像合成"函数一起转成 C，接入既有 `REPL_DLG` 组：

| addr | size | usage | C 名 | 一句话 |
|---|---|---|---|---|
| `0x1956B` | 352 B | 52 | `msg_open_portrait(int id)` | 分配 3×64000 屏缓冲、存屏、画框体、按 id 选 DATO 偏移、镜像 blit 头像、合成 6 条带 |
| `0x1974C` | 153 B | 9 | `msg_blit_band(int y, void *dst, void *src)` | 存档屏拷进工作缓冲、叠加 `y` 行、整屏推回 VGA |
| `0x26996` | 119 B | 42 | `msg_close_portrait(void)` | 合成 1..5 条带、存档屏推回 VGA、`free` 三个缓冲 |

**624 B / 103 个直接调用点**一次进 C。后置的唯一理由（`malloc(64000)×3` 需要 `guest_mem`
这个唯一堆缝）在 §47 已消失；`res.c`/`dlg.c` 已用过该缝。**未闭合的游戏函数 = 0**
（唯一依赖 `0x1974C` 在本簇内，`dlg_box_stage`/`res_load`/`rle2_blit_mirror` 全部已接入）。

- `msgcheck`：**465 cases / 0 failures**（新目标，`build.ps1` + `Makefile.linux`）。
- `regress.ps1`：**8/8 PASS**、`repl: installed 111 → 114`、`FD2.TMP=207360`。
- 同 tick A/B（`--shot-tick=500`）：`none↔none2` **0 / 64000 px**、`none↔all` **0 / 64000 px**。
- 分组开关：`--replace=dlg` 单独开 → `installed 21`（含新增 3 个）。
- Linux：`make -f Makefile.linux` 通过、`letest` 三对象 **exact match**、`doscheck` **49/49**。

---

## 65.2 反编译语义（以机器码为准）

三个函数都被 Hex-Rays 印成 `__fastcall __usercall …(a1..a7)`，那是
`push <frame>; call 0x3702F`（Watcom `_chkstk` 栈探针）的伪像（`rounds/08` §37.3、
`PITFALLS` §8-75）。**真 ABI = cdecl 栈参**，逐个由入口 `mov reg,[esp+…]` 与调用点
`push` 顺序定案：

| 函数 | 真栈参 | 证据 |
|---|---|---|
| `0x1956B` | `(int id)` | 入口 `mov ebx,[esp+4+arg_0]`；52 个调用点各 `push` 1 值 |
| `0x1974C` | `(int y, void *dst, void *src)` | `0x19759/76d/9e/b4` 读 3 个栈位；调用点 `push`×3 |
| `0x26996` | `(void)` | 调用点不压参；函数体不读 arg |

### `0x1956B` `msg_open_portrait`

```
malloc(0xFA00) ×3  -> dword_53C5B / 53C5F / 53C63
memmove(53C5F, 0xA0000, 64000)          ; 存屏
memmove(53C63, 53C5F, 64000)            ; 复制
dlg_box_stage(53C63, 320, 5, 112, 19, 5); push 顺序 (5,19,112,5,320,53C63)
switch(id): 0x80->0x10BB  0x81->0x06AB  0x82->0x0F63
            0x83->0x0576  0x84->0x0E3C  default->0x9017
dword_53A85 = res_load("DATO.DAT", dword_53A85, id)
rle2_blit_mirror(53C63 + dword_53C67,
                 53A85 + *(uint8_t*)53A85, 320)   ; LMI 缓冲首字节=头长
for (i=5; i>=0; --i) msg_blit_band(13*i+112, 53C5B, 53C63)
```

易错点（已逐条钉死）：switch 是**立即数**不是表；`rle2` 的 src 基址要 `+buf[0]`；
目标偏移 `53C63 + 53C67` 是 **32 位整数加法**；**不 free 旧缓冲**（原码泄漏，照抄）。

### `0x1974C` `msg_blit_band`

```
n = 86
memmove(dst, 53C5F, 64000)              ; 从存档屏起步
if (y+86 >= 200) n = 200 - y            ; cmp eax,0C8h; jl（有符号）
for (i=0; i<n; ++i)
    memmove(dst + 320*i + 5 + 320*y,
            src + 320*i + 0x8C05, 310)  ; 0x8C05 = 112*320+5
memmove(0xA0000, dst, 64000)            ; 推回 VGA（dst 不是 src）
; jmp 0x16F04 = dlg_scroll_text 的共享尾声 -> C 里 return
```

### `0x26996` `msg_close_portrait`

```
for (i=1; i<6; ++i) msg_blit_band(13*i+112, 53C5B, 53C63)  ; 只 1..5
memmove(0xA0000, 53C5F, 64000)
free(53C5B); free(53C5F); free(53C63)   ; 不判空、不清零
```

---

## 65.3 实现（`src/game/msg.c` / `.h`）

- 三个 64000 B 缓冲留在**原地址全局** `dword_53C5B/F/63`（另有 48/62/91 个未转译点共享），
  C 读写真正的原字；模块只碰 `dword_53A85`/`dword_53C67` 两个相邻全局。
- **堆走 `guest_mem`**（`guest_malloc`/`guest_free`）——这是本簇此前被后置的唯一原因，
  现在只有这一条缝，host 与 check 共用一堆。
- 服务走**原地址**（scene.c 模式）：`BOX_STAGE`=0x168B6、`RES_LOAD`=0x111BA、
  `RLE2_MIRROR`=0x4EC31、`BLIT_BAND`=0x1974C，用 `typedef`+`#define` 宏，这样 host 里它们
  已被 `repl.c` patch 成 C、check 里被钩成记录桩，两条路径都对。
- `memmove` 直接用宿主 libc（`0x3771C` 只对机器码侧有意义）。
- 不复制 `0x16F04` 共享尾声道具：C 只写 `return`。

### 接入边界的一致性

`0x1974C` 另有 7 个直接调用点（主要来自未转译的 `0x196CB` 等）；接线后它们也进 C——
这是 `repl` 的预期行为。三个函数共享同一组全局缓冲指针，中途分组切换语义不完整，
因此统一放 `REPL_DLG`（原子开关）——`--replace=dlg` 实测 `installed 21`。

---

## 65.4 对拍 harness（`src/msgcheck.c` → target `msgcheck`）

复用 `boxcheck` 骨架（`le_reserve_address_space` → `le_open` → `le_map_and_relocate` →
5 字节 jmp 钩子）。每例：跑原机器码 → 恢复世界 → 跑 C → 比较。

**钩子**：`0x3706E`/`0x3776E`（CRT malloc/free，记录 + 分配登记）、`0x168B6`（记录盒体
6 个实参）、`0x111BA`（返回受控 DATO、记录 `old`/`index`）。**`0x1974C` 故意不钩**：
open/close 两侧就都跑**真条带机器码**，而条带本身在直测里单独原码 vs C 对拍。

**比较**（全部逐字节 / 逐整数）：
1. 事件序列（alloc/free/box_stage/res_load）的**全部标量实参 + 顺序**；指针实参按
   “分配登记表的 (块号, 偏移)”归一化（两次运行 malloc 地址不同，不能比原值）；
2. 原全局 `dword_53C67` / `dword_53A85`；
3. 整幅 VGA（`0xA0000..+64000`）；
4. 三块屏缓冲（`53C5B`/`53C5F`/`53C63`）与 DATO 缓冲逐字节；
5. `msg_blit_band` 直测另比 `dst`/`src`/`53C5F` 三块缓冲 + VGA。

**用例矩阵（465）**：

| 测试 | 维度 | 小计 |
|---|---|---|
| `msg_blit_band` 直测 | `y ∈ {0,1,85,86,112,113,114,150,199,200,201,255}` × 20 随机 | 240 |
| `msg_open_portrait` | `id ∈ {0x7F,0x80,0x81,0x82,0x83,0x84,0x85,0x00,0xFF}` × 20 随机 | 180 |
| `msg_close_portrait` | 40 组随机屏对 | 40 |
| `open` 后 `close` | 5 组 | 5 |

**故障注入验证**（确认 harness 真能抓错）：把 `BAND_SRC_OFF` 从 `0x8C05` 改成 `0x8C04`
→ `band dst`/`band vga` 立即报 `@+5`；把 `BOX_STAGE` 的 `y0` 112 改成 113 →
`event 3 box_stage(...112...)` vs `(...113...)`。两次注入都被当场抓住，随后还原。

---

## 65.5 验收数据

| 判据 | 结果 |
|---|---|
| `msgcheck` | **465 cases, 0 failures** |
| 既有 22 个 `*check` | 全过（rle 1900、gfx 1450、sprite24 2100、util 2200、path 1000、res 160、tables 4528、rle2 1200、dlg 800、box 240、key 100、keys 102、rec 42225、type 1616、vm 5512、leaf 72008、fade 4000、map 97000、scene 1480、bgm 6000、fx 12884、dos 49/49） |
| `regress.ps1` | **8/8 PASS**、`FD2.TMP=207360`、`repl: installed 114`（mask 0x7FFF） |
| 静态帧 A/B `--shot-tick=500` | `none↔none2` **0/64000 px**、`none↔all` **0/64000 px** |
| `--replace=dlg` | `installed 21` |
| Linux | `make -f Makefile.linux` 通过、`letest` 三对象 **exact match**、`doscheck` **49/49** |
| `translation_map.py --check` | 通过（**114 wired / 1359 = 8.4%**） |

> Linux 全量首次编译有一条**既有** `map.c` 的 `-Wint-to-pointer-cast`（64 位链接证明侧，
> 本文件未改动，与 `git status` 一致）；`msg.c` 本身 0 warning。

---

## 65.6 备注 / 边界

- **不改** `repl.h`、**不加**新 `REPL_*` 分组、不重构 `repl.c` 表结构。
- 严格照抄原码：覆盖旧缓冲指针**不 free**、`close` 后**不清零**全局——逐字节对齐优先。
- 不动 `0x2C67D`（`funcs_30469[6]`，含 CRT 浮点）、不动调用 `0x1974C` 的大函数
  `0x196CB`/`0x197E5`/`0x19953`、不动 `funcs_1199C/1197B` 表项。
- 下一批：`funcs_1199C`(`0x51B91`) 表里那批同族 80 字节记录服务
  （`[0]=0x34531`/`[1]=0x3460B`/…）、`funcs_30469[6] 0x2C67D`。
