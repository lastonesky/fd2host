# 第 36 轮：`funcs_1199C` 事件 handler 闭合子集（§66，第 115–125 个）

> 轮次入口：`build/agents/next_translation.md`（规划 agent 的分配）。
> 真源：`E:\FD2\FD2.EXE.i64`（ida MCP）、`src/repl.c`、`re/func_ranking.csv`。
> 判据：`src/evcheck.c` 事件序列 + 记录表逐字节 + 返回值对拍 + `regress.ps1` 8/8 +
> 同 tick A/B 0 px + Linux `letest` exact match / `doscheck` 49/49。

---

## 66.1 目标与结论（一句话）

把 `funcs_1199C`（`0x51B91`）分派表里**依赖闭合的 11 个 handler** 一次性转成 C，
新增模块 `src/game/ev.c/.h`，接入既有 `REPL_REC` 组（不新增 `REPL_*`、不重构表）：

| addr | idx | size | C 名 | 一句话 |
|---|---|---|---|---|
| `0x34738` | [4] | 64 | `ev_rec13_set1` | 记录 13 `+6=1`，`vm_run(sub=7)` |
| `0x348EA` | [12] | 86 | `ev_status24_27` | 一次性：`status(24,27,7)` + `vm_run(sub=3)` + 置位 |
| `0x34A6C` | [19] | 155 | `ev_status7_36` | `status(7,36,7)` + `vm_run(sub=8)` + 扫 `rec_flag(7..36)`，有 0 再 `vm_run(sub=11)` |
| `0x34B2F` | [21] | 64 | `ev_flag8_gate` | `rec_flag(8)==0` 时 `vm_run(sub=2)` |
| `0x34CF1` | [26] | 62 | `ev_rec6_gate` | 记录 `idx` `+6!=0` 时 `status(9,27,0)` + 置位；**双返回语义** |
| `0x34D92` | [29] | 62 | `ev_mask_records` | `vm_run(sub=2)`，`return rec_status_mask_records()` 的 EAX |
| `0x34F74` | [33] | 78 | `ev_clear_status12_13` | `vm_run(sub=2)`，记录 12/13 `+52=0`，返回 `base+1040` |
| `0x35123` | [8] | 110 | `ev_slot8_claim` | `arg==0 && slot_free()!=8 && !flag` → `claim(0,89)` + `vm_run(sub=11)` + 置位 |
| `0x35191` | [10] | 85 | `ev_status16_71` | 一次性：`status(16,71,0)` + `vm_run(sub=1)` + 置位 |
| `0x351E6` | [13] | 114 | `ev_clear64_73` | `vm_run(sub=6)`，记录 64..73 `+53=0`，`status(64,73,3)`、`status(35,49,0)` |
| `0x35258` | [18] | 64 | `ev_status16_34` | `vm_run(sub=8)`，`status(16,34,0)` |

**944 B / 11 个函数 / 每个 9 个到达点（8 个函数 + `funcs_1199C` 表项）**。全部是纯整数事件序列：
零堆、零文件、零浮点、零 VGA 直写；只依赖**已接入**的 `vm.c` / `rec.c` 服务。**未闭合依赖 = 0**
（唯一"外部"是 `0x3702F` 栈探针，C 不需要）。

- `evcheck`：**2940 cases / 0 failures**（新 target、新 `src/evcheck.c`）。
- 故障注入 3 次全部当场抓到（见 §66.4）。
- `regress.ps1`：**8/8 PASS**、`repl: installed 114 → 125`、`FD2.TMP=207360`。
- 同 tick A/B（`--shot-tick=500`）：`none↔none2` **0 / 64000 px**、`none↔all` **0 / 64000 px**。
- 分组开关：`--replace=rec` 单独开 → `installed 29`（旧 `rec` 组 18 + 新增 11，`mask 0x40`）。
- Linux：`make -f Makefile.linux` 通过（无新 warning）、`letest` 三对象 **exact match**、`doscheck` **49/49**。
- `translation_map`：**125 / 1359（9.2%）**。

---

## 66.2 反编译语义（以机器码为准）

### 66.2.1 真 ABI：cdecl 栈参，一参由分派器压入、调用者清栈

每个入口都是 `push <帧>; call 0x3702F`（Watcom `_chkstk` 栈探针），Hex-Rays 印的
`__usercall …@<eax/edx/ecx/ebx/edi>` 是它的伪像（`PITFALLS` §8-75）。`0x3702F` 的机器码是

```asm
3702F  xchg eax,[esp+4]   ; 保存原 EAX，EAX := 帧大小
37033  call 0x37042       ; 真正的栈探针
37038  mov  eax,[esp+4]   ; EAX 还原
3703C  retn 4             ; pop 返回址 + 4（帧大小）
```

⇒ 函数返回后的 `[esp+4]` 就是第一个（也是唯一）实参；分派器 `sub_117E7` 的
`push <arg>; call funcs_1199C[…]` 后面跟 `add esp,4`，**由调用者清栈**。
11 个函数里只有 `0x34CF1`、`0x35123` 读这个实参，其余 9 个写成 `(void)` 是精确的。

### 66.2.2 返回值：4 个真 `void`、7 个有定义 —— 逐个由 `retn` 前的 EAX 定案

`0x3702F` **保持 EAX**（见上），所以函数入口的 EAX 就是**调用者的 EAX**。对提前返回的路径，
EAX 可能是调用者的垃圾值而非游戏值。实测所有 9 个调用点（`sub_117E7` 两处 + 6 个函数，
全是 `call funcs_1199C[eax*4]; add esp,4`）**都不读 EAX**，于是：

| addr | retn 时 EAX | C 签名 |
|---|---|---|
| `0x34738` | `vm_run` 的返回值（VGA 地址，被丢） | `void` |
| `0x34A6C` | `hit==1 ? vm_run 返回值 : 0` | `void` |
| `0x34B2F` | `rec_flag(8)` 或 `vm_run` 返回值 | `void` |
| `0x35123` | `arg!=0` 时是**调用者 EAX**；`slot_free==8` 时 8；`flag!=0` 时 flag 字节；触发时 `dword_53AD5` | `void`（见 §66.2.4） |
| `0x348EA` | flag 字节（非 0）或 `dword_53AD5`（触发后） | `uint32_t` |
| `0x34CF1` | 记录 `+6` 字节（0）或 `dword_53AD5` | `uint32_t` |
| `0x35191` | flag 字节（非 0）或 `dword_53AD5` | `uint32_t` |
| `0x34D92` | `rec_status_mask_records()` 的 EAX = 记录表基址 | `uint32_t` |
| `0x34F74` | `dword_53A45 + 0x410`（记录 13） | `uint32_t` |
| `0x351E6` | 第二次 `status(35,49,0)` 的 EAX（真实 `0x344F2` 返回 `base + 80*49`） | `uint32_t` |
| `0x35258` | `status(16,34,0)` 的 EAX（`base + 80*34`） | `uint32_t` |

> `0x344F2` 的 EAX 是**循环变量算出的地址** `dword_53A45 + 80*last_index`（`0x34516`
> `mov ebx,dword_53A45` 后 `add eax,ebx`，写回后 `inc/cmp` 退出时 EAX 仍是它）——
> 所以 `ev_clear64_73`/`ev_status16_34` 的 C 直接把这些调用的返回值转出。

### 66.2.3 全局与记录表

- `dword_53A45`：80 字节记录表基址。本批只做**直接字节写**：
  `0x34738` → `base[13*80+6]=1`；`0x34F74` → `base[12*80+52]=0`、`base[13*80+52]=0`；
  `0x351E6` → `for(i=64;i<=73;i++) base[i*80+53]=0`（**`+53`，不是 `+52`**）。
- `dword_53AD5`：状态块基址，`+16` 是**一次性触发标志**（读-判零-置 1），
  `0x348EA`/`0x34CF1`/`0x35123`/`0x35191` 都做 `if (*(uint8_t*)(G+16)==0){…;*(G+16)=1;}`。
- `dword_53A79`：VM 脚本容器指针，`vm_run` 第 1 参。

### 66.2.4 `0x35123`：机器码把**传入实参**当 `rec_slot_free`/`rec_slot_claim` 的索引

规划稿 §2.5 写“`rec_slot_free` 的记录索引用 `dword_53AD5[16]`（此处为 0）”——**不准确**：
机器码 `0x35134 push [esp+arg_0]` / `0x35154 push [esp+8]`（此时 `esp` 已因 `add esp,4`
上移 4，故 `[esp+8]` 仍是入口的 `arg_0`）读的就是**分派器压入的那个实参**。
因为三门前置已经 `cmp [esp+arg_0],0; jnz` 排除了非 0，运行时 `arg_0 == 0`，
`rec_slot_free(0)`/`rec_slot_claim(0,89)` 与“写成 0”等价。C 照机器码写成
`ORIG_SLOT_FREE(idx)` / `ORIG_SLOT_CLAIM(idx, 89)`（`idx==0`）。
`arg != 0` 的提前返回 EAX 是调用者 EAX（§66.2.2），所以本函数定 `void`。

---

## 66.3 实现（`src/game/ev.c` / `.h`）

- 服务**走原地址**（`scene.c`/`msg.c` 模式）：`ORIG_VM_RUN`(`0x15F84`)、`ORIG_STATUS`(`0x344F2`)、
  `ORIG_REC_FLAG`(`0x34894`)、`ORIG_SLOT_FREE`(`0x1B8A6`)、`ORIG_SLOT_CLAIM`(`0x1BB8C`)、
  `ORIG_MASK`(`0x34D64`)。宿主里这些地址已被 `repl.c` patch 成 C，check 里被钩成记录桩，
  **两条路径同一份 C**。不 `#include "rec.h"`/`"vm.h"`（避免 check 侧绕过记录桩、
  丢失对拍面）。
- 全局留在原地址（`dword_53A45`/`dword_53AD5`/`dword_53A79`），C 必须读写真字。
- `vm_sub(sub)` 收拢 `vm_run(dword_53A79, sub, 0xA0000, 320, 205, 76, 74, 19, 1)` 的 8 个固定尾参。
- `ORIG_STATUS` 以 `uint32_t (*)(int,int,int)` 声明，专门给 `0x351E6`/`0x35258` 转发 EAX；
  宿主里 `rec_status_set` 是 `void`，返回值调用方不读，无副作用。
- `repl.c`：11 行条目进 `REPL_REC`（注释标明 `funcs_1199C[idx]`），`fd2host` 源表加 `game\ev.c`，
  `Makefile.linux` 的 `GAME_SRCS` 加 `game/ev.c`。

---

## 66.4 对拍 harness（`src/evcheck.c` → target `evcheck`）

6 个服务全部钩成记录桩（`0x15F84`/`0x344F2`/`0x34894`/`0x1B8A6`/`0x1BB8C`/`0x34D64`）：

| 地址 | 桩行为 |
|---|---|
| `0x15F84` | 记 9 个标量实参（`stream` 归一化成对受控缓冲的偏移），返回固定 `0xA0000` |
| `0x344F2` | 记 `(start,end,value)`，**不真写**；返回 `dword_53A45 + 80*end`（复刻真 EAX） |
| `0x34894` | 记 `(index)`，返回受控 profile（全 0 / 全 1 / 单点 0 / 随机） |
| `0x1B8A6` | 记 `(index)`，返回受控 0..8 |
| `0x1BB8C` | 记 `(index,value)`，返回受控 1 |
| `0x34D64` | 记无参调用，返回 `dword_53A45`（复刻真 EAX） |

世界：`rec_in`（256 条 × 80 B）作模板，`rec_o`/`rec_c` 逐字节相同的两份副本分别给
原机器码 / C（`dword_53A45` 指各自那份）；`dword_53AD5` 是同一个受控 64 B 块（`+16 ∈ {0,1,0xFF}`、
`+21` 随机）。指针返回值归一化成 `ret - base` 再比。

**用例矩阵 ≈2940**：

| 组 | 维度 | 小计 |
|---|---|---|
| 9 个无参 handler | `ad5[16] ∈ {0,1,0xFF}` × `slot_free ∈ {0,4,8}` × 20 随机 | 1620 |
| `0x34CF1` | `idx ∈ {0,12,13,63,64,73,219,255}` × `record[+6] ∈ {0,1,0x80,0xFF}` × 20 | 640 |
| `0x35123` | `arg ∈ {0,1,0xFF}` × `slot_free ∈ 0..8` × `ad5[16] ∈ {0,1}` × 10 | 540 |
| `0x34A6C` profile | 全 1 / 全 0 / 窗口两端单点 0 × 20 | 80 |
| `0x351E6` 窗口 | 记录 63/64/73/74 的 `+53` 边界 + 随机（窗口外不动） | 40 |
| `0x34F74` 边界 | 记录 11/12/13/14 的 `+52` + 随机 | 20 |

**故障注入（harness 自检，3/3 当场抓到）**：

1. `ORIG_STATUS(24,27,7)` → `(24,27,0)`：事件序列报 `event 0 status_set … 7/0`，
   **60 failures**。
2. `ev_clear64_73` 的 `+53` → `+52`：记录表在字节 5172（记录 64 偏移 52）报
   `orig=86 ours=00`，**220 failures**。
3. `ev_rec6_gate` 触发分支 `return dword_53AD5` → `return b`：`cmp_read` 报
   `return orig=…AD5 ours=FF`，**480 failures**。

---

## 66.5 验收数据

| 判据 | 结果 |
|---|---|
| `evcheck` | **2940 cases / 0 failures** |
| 故障注入 | 3/3 被抓（60 / 220 / 480 failures） |
| `regress.ps1` | **8/8 PASS**、`FD2.TMP=207360`、`repl: installed 125` |
| `--replace=rec` | `installed 29`（`mask 0x40`；旧 18 + 新增 11） |
| A/B 同 tick `--shot-tick=500` | `none↔none2` **0/64000 px**、`none↔all` **0/64000 px** |
| Linux | `make -f Makefile.linux` 通过、`letest` 三对象 **exact match**、`doscheck` **49/49** |
| `translation_map` / `func_ranking` | **125 wired / 1359（9.2%）**（`--check` 通过） |

---

## 66.6 备注 / 边界

1. **未碰** `funcs_1199C` 另外 34 项（含被上一轮点名的 `[0] 0x34531`/`[1] 0x3460B`）：
   它们都要求先转约 **13 KB 的"场景渲染核"**（`0x10B4E`/`0x135DD`/`0x1366A`/`0x11CAC`/
   `0x11EEE`/`0x122DC`/`0x1ACF3` 及其传递闭包），单独立项。
2. **未 hook 尾块** `0x353E7`/`0x353FA`/`0x36416`/`0x3642E`：它们 `jmp 0x3528D/0x3528F`，
   落在 `0x35258` **函数体中部**（"无 `vm_run` 的 status 写入"变体），不是真入口；
   只 patch `0x35258` 入口，尾块继续跑原机器码。
3. **不改** `repl.h`、不新增 `REPL_*`、不动 `rec.c`/`vm.c` 既有实现。
4. **规划稿的一处误读**（§66.2.4）已在实现里按机器码纠正；`0x35123` 的 C 保留了
   `ORIG_SLOT_FREE(idx)`/`ORIG_SLOT_CLAIM(idx, 89)`，与机器码一致。
5. Linux 侧沿用分级：纯游戏逻辑转译只做构建 + `letest`/`doscheck`（无平台/渲染/入口改动）。
