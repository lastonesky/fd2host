# 轮次明细：`funcs_25E23[14]` 状态 handler + 队伍身份查询叶子（第 94–95 个转译函数）

§61 按规划 agent 的分配，把 `main`（`0x25BF4`）主循环 `call funcs_25E23[dword_53C03]`
转移分派表 `funcs_25E23 @0x51DE9` 的**第 6 个表项** `[14]` = `0x239BD` 还原成 C，
并补上它**唯一**未转译的依赖 `0x33499`（队伍身份查询）。合计 **141 B obj0 机器码 /
4 个到达点（3 调用 + 1 表项指针）**，闭包 = `{0x239BD, 0x33499}` 两个函数：
`0x239BD` 的其余被调（`vm_run`/`unit_refresh_all`/`unit_add`）全部已接入。
扩进既有 `src/game/scene.c`（handler）+ `src/game/unit.c`（叶子）+ 既有
`src/scenecheck.c` / `src/reccheck.c`，接入既有分组 `REPL_SCENE` / `REPL_REC`。
`scenecheck` **1100 → 1480/0**、`reccheck` **36327 → 37398/0**；`regress` **8/8**、
`repl: installed 95`、静态帧 A/B **0 px**。

---

## 61.1 这一批

| addr | size | usage | 表项 | C 名 | 语义 |
|---|---|---|---|---|---|
| `0x239BD` | 77 | 4 | `funcs_25E23[14]` | `scene_state_14` | `unit_exists(12)` → `sub=(al^1)+12`（12/13）→ `vm_run(流,sub,…)` → `unit_refresh_all` → `unit_add(15)` → `dword_53C03++` |
| `0x33499` | 64 | 7 | — | `unit_exists` | 扫队伍表 `dword_53BF7` 起 `dword_53BFB` 条 80 B 记录，`movzx byte[+8] == id` 命中返回 1，否则 0 |

- `0x239BD` 的 3 个直接调用点：`0x25E23`（`main` `0x25BF4`）、`0x1541F`（函数 `0x15311`）、
  `0x1D479`（函数 `0x1CFF0`）；另有 1 个 `data_xrefs` = 表项指针 `funcs_25E23[14] @0x51DE9+4*14`。
  入口 patch 后 4 个到达点全部进 C。
- `0x33499` 的 7 个直接调用点：`0x1B5B7`(`0x1B41D`)、`0x20883`(`0x20872`)、`0x239C9`(`0x239BD`)、
  `0x23B93`(`0x23B5F`)、`0x2B2C6`(`0x2AF28`)、`0x334EB`(`0x334D9`)、`0x335BB`(`0x335AA`) —— 是共享叶子。

### 61.1.1 IDA 逐条核对（`E:\FD2\FD2.EXE.i64`）

`0x239BD`（逐条核对，`E:\FD2\FD2.EXE.i64`）：

```asm
0x239bd  push    28h                ; Watcom 栈探针参数，真实 ABI = void fn(void)
0x239c2  call    sub_3702F          ; _chkstk，CRT，约定不转译
0x239c7  push    0Ch                ; id = 12
0x239c9  call    sub_33499          ; → al = unit_exists(12) ∈ {0,1}
0x239ce  add     esp, 4
0x239d1  xor     al, 1              ; al = exists ^ 1
0x239d3  add     al, 0Ch            ; al = (exists^1) + 12  → 12 或 13
0x239d5  push    1                  ; vm_run arg9  wait
0x239d7  push    13h                ; arg8  line_step = 19
0x239d9  push    4Ah                ; arg7  bgfill   = 74
0x239db  push    4Ch                ; arg6  shadow   = 76
0x239dd  push    0CDh               ; arg5  fg       = 205
0x239e2  push    140h               ; arg4  pitch    = 320
0x239e7  push    0A0000h            ; arg3  addr     = 0xA0000 (VGA)
0x239ec  movzx   eax, al
0x239ef  push    eax                ; arg2  sub
0x239f0  push    dword_53A79        ; arg1  stream
0x239f6  call    sub_15F84          ; vm_run(...9 args...)
0x239fb  add     esp, 24h
0x239fe  call    sub_11506          ; unit_refresh_all()
0x23a03  push    0Fh                ; unit_add 实参 = 15
0x23a05  jmp     loc_237C8          ; 共享尾：call unit_add; add esp,4; jmp 0x231F2
```

- **真实 ABI**：`void scene_state_14(void)`（`push 28h; call 0x3702F` 是 `_chkstk` 伪像，
  与第 30 轮五件套同一现象，见 `rounds/08` §37.3）。
- **共享尾跳**：`0x239BD` 落到 `0x237C8` = `call unit_add(15); add esp,4; jmp 0x231F2`，
  而 `0x231F2` = `inc dword_53C03; retn`。注意 `0x237C8` 位于 `0x23790`（已接入的
  `scene_state_10`）**函数体内部**：`repl.c` 只覆盖 `0x23790` 的**入口 5 字节**为 jmp，
  共享尾 `0x237C8` 仍是原机器码，跳到那里执行 `call 0x112A5`（已被 patch 成 C）完全成立。
  C 按语义写 `UNIT_ADD(15); dword_53C03++;`，**不复刻 jump 布局**。
- 读写的全局：`dword_53A79`（VM 流指针，只读）、`dword_53C03`（状态索引，自增）。

`0x33499`（逐条核对）：

```asm
0x33499  push    8
0x3349e  call    sub_3702F          ; _chkstk
0x334a3  push    ebx
0x334a4  mov     ecx, [esp+4+arg_0] ; id（cdecl 栈参数）
0x334a8  xor     edx, edx           ; i = 0
0x334aa  jmp     short loc_334AD
0x334ac  inc     edx
0x334ad  cmp     edx, dword_53BFB   ; 有符号比较（jge = 有符号）
0x334b3  jge     short loc_334D5
0x334b5  mov     eax, edx
0x334b7  shl     eax, 2
0x334ba  lea     ebx, [edx+eax]     ; ebx = i*5
0x334bd  shl     ebx, 4             ; ebx = i*80 = 记录字节偏移
0x334c0  mov     eax, dword_53BF7   ; 队伍表基址（每轮重读）
0x334c5  movzx   eax, byte ptr [ebx+eax+8]  ; 记录 +8 = 身份字节
0x334ca  cmp     eax, ecx           ; 与完整 32 位 id 比较
0x334cc  jnz     short loc_334AC
0x334ce  mov     eax, 1             ; 命中 → 1
0x334d3  pop     ebx
0x334d4  retn
0x334d5  xor     eax, eax           ; 未命中 → 0
0x334d7  pop     ebx
0x334d8  retn
```

- **真实 ABI**：`int unit_exists(int id)`（cdecl，一个栈参数，`int` 返回）。
- 无副作用：不写任何全局、不写表，只读 `dword_53BF7`/`dword_53BFB`。

### 61.1.2 三个易错点（差分对拍钉住）

1. **`sub` 来自被调返回值，不是常量**：`vm_run` 的第 2 实参是
   `(uint8_t)((unit_exists(12) ^ 1) + 12)`——存在 → **12**，不存在 → **13**。
   原机器码只取 `al`（`xor al,1; add al,0Ch; movzx eax,al`），C 用 `uint8_t` 截断
   保证对任意返回值都等价（对 0/1 以外的值也成立：低字节运算的模性质）。
2. **顺序**：`vm_run` → `unit_refresh_all` → `unit_add` → `dword_53C03++`，
   事件对拍能钉住调用次数与顺序。
3. **`unit_exists` 的 id 不能被 `uint8_t` 截断**：`movzx` 后与**完整 32 位** `id` 比较，
   所以 `id > 255`（如 256）永远不命中（`id=256` 不能错误命中原 `record[+8]==0`）；
   `id` 为负也不命中。循环边界是**有符号** `jge`，`dword_53BFB < 0` 直接返回 0。

---

## 61.2 对拍（`scenecheck` + `reccheck` 扩展）

### 61.2.1 `scenecheck`（handler，记录桩）

沿用既有骨架（`le_reserve_address_space` → `le_open` → `le_map_and_relocate`；`install_hook`
5 字节 `jmp`；事件日志逐调用比较）与既有 6 handler 各 200 例。本簇新增 1 个记录桩：

| hook | 记录 |
|---|---|
| `0x33499` `stub_unit_exists` | 事件 `EV_UNIT_EXISTS`，实参 `id` + 返回的全局 `g_exists`；返回 `g_exists` |

- 另 3 个服务沿用既有 `stub_vm`/`stub_unit_refresh`/`stub_unit_add`。
- `scene_state_14` 加进 `g_handlers`（`uses_exists=1`，`g_exists` 随 rep 交替 0/1）= 200 例；
  再用 `exists ∈ {0,1,0x100,0x101,0xFF,-1}` × 30 rep = 180 例，专门钉住
  “原机器码用 `al`、C 用完整 int 后截断”的等价性（对 >255、负数返回值也必须一致）。
- 判据三件套：① 事件序列（次数 + 每个实参，`EV_VM` 的 `sub` 证明用的是 12/13）；
  ② `dword_53C03` 终值（含 `0x7FFFFFFF`/`-1` 回绕）；③ 1 KiB 全局快照逐字节相等。
- `0x239BD` 的共享尾跳 `0x237C8` 是原机器码，`call 0x112A5` 被既有 `stub_unit_add` 捕获 ⇒
  与 C 的 `UNIT_ADD(15)` 对齐。

**判据**：`scenecheck` **1100 → 1480/0**。

### 61.2.2 `reccheck`（叶子，真队伍表）

复用既有 party 表设施（`G53BF7`/`G53BFB`、`REC_STRIDE`(80)、`tbl2`/`obuf`/`cbuf`、`rnd()`、
`MAXREC`）。两侧在**同一张表的两份逐字节副本**上跑，比较返回值 + 整张表逐字节
（证明运行期不改表）：

| 组 | 构造 | 数量 |
|---|---|---|
| 随机 | `n2∈0..64`、随机 `+8`、`id∈0..255`，含强制命中/多命中（取首个） | 1050 |
| 边界 | 空表；命中在第 0/中/末条；`+8` 全同 vs 唯一；want 0/255 | 9 |
| 宽 id | `256/257/300/0x7FFFFFFF/-1/-256`（`movzx` 不命中、id 不截断） | 6 |
| 负计数 | `dword_53BFB ∈ {-1,-5,INT_MIN}`（有符号边界，返回 0） | 6 |

**判据**：`reccheck` **36327 → 37398/0**（Δ +1071）。

---

## 61.3 接入

`src/repl.c` 追加两行，沿用既有分组：

- `{ 0x33499, "unit_exists", (void *)unit_exists, REPL_REC }`（与 `unit_recalc/refresh_all/add` 同组）
- `{ 0x239BD, "scene_state_14", (void *)scene_state_14, REPL_SCENE }`

两组独立：`--replace=scene` 单独开时 `0x33499` 仍是机器码，C 的 `UNIT_EXISTS` 走原地址照常工作；
`--replace=rec` 单独开时 `0x239BD` 仍机器码。

---

## 61.4 判据

| 项 | 结果 |
|---|---|
| `scenecheck` | **1480/0**（1100 → +380） |
| `reccheck` | **37398/0**（36327 → +1071） |
| `regress.ps1` | **8/8 PASS**、`repl: installed 95`、`FD2.TMP=207360` |
| A/B 同 tick `--shot-tick=500` | `none↔none2` **0 / 64000 px**、`none↔all` **0 / 64000 px** |
| Linux | `make -f Makefile.linux` 构建通过、`letest-linux` 三对象 exact match、`doscheck-linux` **49/49** |
| `translation_map` / `func_ranking` | **95 wired / 1359（7.0%）**（`--check` 通过；1114 未转译） |

进度 **95 / 1359（7.0%）**。

## 61.5 一起踩的坑

- **从 Git Bash 给 `pwsh -File ab_run.ps1 -Bmp` 传 Windows 路径，反斜杠被 bash 吃掉**：
  首次 A/B 三条命令都“跑完”了，但 `host.log` 显示图写到了
  `E:FD2portbuildab_n1.bmp`（畸形路径），`build\ab_*.bmp` 不存在，`framediff` 报 `not found`
  —— 看着像 `--screenshot` 失效，其实是 **bash 未加引号时把 `\F`、`\p`… 当转义**。
  给 `-Bmp` 的值加**单引号**后正常。记 `docs/PITFALLS.md` §8-74。

## 61.6 下一步

`funcs_25E23` 分派表 25 项里已完成 6 项（`[0]/[3]/[10]/[12]/[14]/[18]`）；其余 19 项
（`0x22F37`/`0x230F2`/`0x231F9`/`0x23296`/`0x232E8`/`0x234BB`/`0x235BC`/`0x235F9`/`0x237D5`/
`0x238DC`/`0x23A0A`/`0x23B5F`/`0x23CD5`/`0x23E74`/`0x240FA`/`0x244B6`/`0x24754`/`0x24C1E`/
`0x24DF2`）闭包均 > 3 KB（含 `0x122DC` 1051 B / `0x11EEE` 885 B / `0x10C50` 969 B 等大函数或
文件 I/O），需先闭各自闭包，另开轮次。并行可进的同族叶子（`rec.c` + `reccheck`）：
`0x1B8A6`/`0x1B83D`/`0x1CA89` 及记录单字节置位叶子 `0x13512`/`0x32975`/`0x34D64`/`0x35009`
（`rounds/28` §58.5）。
