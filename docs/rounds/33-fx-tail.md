# 轮次明细：`funcs_30469` 效果动画表收尾（剩余 5 handler + 1 辅助，第 100–104 个转译函数）

§63 是第 32 轮（`rounds/32-fx-handlers.md` §62）的**同族收尾**：把 `funcs_30469 @0x524C6`
（10 项函数指针表）剩余依赖闭合的 5 个 handler + 1 个 86 B 辅助还原成 C：

| 表项 | addr | size | C 名 | 语义 |
|---|---|---|---|---|
| `[0]` | `0x2B996` | 413 | `fx_dots7` | 7 粒子；7 字节方向表 `0x524EE`，表偏 +148 |
| `[1]` | `0x2BB33` | 569 | `fx_dots8` | 8 粒子；`0x5252D`/`0x5254D`，表偏 +148，像素偏 +80 |
| `[2]` | `0x2BD6C` | 535 | `fx_blob` | 无粒子数组；三字节状态 + 调 `fx_advance` |
| 辅助 | `0x2BF83` | 86 | `fx_advance` | `(phase*,counter*,dst,pitch,buf)`，blit 一帧并推计数器 |
| `[3]` | `0x2BFD9` | 574 | `fx_dots12` | 12 粒子 + 3 个轮转槽；表偏 +20 |
| `[5]` | `0x2C441` | 572 | `fx_dots6b` | 6 粒子；10 dword 表 `0x525DD`，表偏 +143 |

合计 **2749 B**，usage 77（5×15）+ 辅助 2 个调用点。接入既有分组 **`REPL_FX`**（不新增），
新模块沿用 `src/game/fx.c/.h`，对拍沿用 `src/fxcheck.c`（不新建 target）。

`fxcheck` **3920 → 12884 / 0**（新增 8964 例）；既有 22 个 `*check` 全部维持通过；
`regress` **8/8**、`repl: installed 99 → 104`、`FD2.TMP=207360`；静态帧 `--shot-tick=500`
A/B **0/64000 px**；Linux 构建通过 + `letest` 三对象 exact match + `doscheck` **49/49**。

---

## 63.1 为什么是这一批 / 闭合价值

1. **第 32 轮显式交接的下一步**（`rounds/32` §62.6）：同表同族，模板现成。
2. **收尾一张完整分派表**：转完该表 **9/10**，唯一剩余 `[6] 0x2C67D`（1151 B，含 CRT
   `cos/sin`）单列一轮。`sub_31266`（632 B 小分派器）因此只差 `0x2C67D` + `0x2FB2C`(744)
   + `0x2FE14`(237)。
3. **零新缝**：不碰文件 I/O / 堆 / AIL / 时钟；只读写游戏数据段全局、从**原地址** `memcpy`
   只读表、调 4 个**已接入**服务（`res_blit`/`svc_play_sfx(2)`/`util_rand`）。
4. **一个入口 patch 覆盖 15 个到达点**：`callers_game = 14` 是 14 个间接分派点
   （`sub_2FF01` 10 处 + `sub_31266` 4 处，`call funcs_30469[reg*4]`），不是 14 个函数。

---

## 63.2 真实 ABI 与陷阱（先读）

六函数全部被 Hex-Rays 印成 `__fastcall …(a1..a9)`——`push <frame>; call 0x3702F`
（Watcom `_chkstk`）栈探针的伪像，前 4 个"寄存器参数"是垃圾（`rounds/08` §37.3、
`rounds/32` §62.4、`docs/PITFALLS.md` §8-75）。**真 ABI = cdecl 栈参**：

```c
/* 5 个 handler：与已转的 4 个同签名 */
int f(int rec, const void *buf, void *dst, int pitch, int selector);
/* 辅助 0x2BF83：另一套 5 参 */
int fx_advance(uint8_t *phase, uint8_t *counter, void *dst, int pitch, const void *buf);
```

- 实参定案一律回到 `--dump` 的 `push` 顺序：handler 内 `res_blit` 恒为
  `push -1; push [arg_C=pitch]; push <dst 表达式>; push <index>; push [arg_4=buf]`。
- `selector` 是 **byte 载入**（实测 `0F B6`，`movzx eax, byte ptr [arg_10]`），
  故 `(uint8_t)selector`；harness 用 `0x100/0xFF/0x12/0x1FF` 钉死这一点（若误用 word，
  `0x100` 会走 default 而不是 case 0，diff 立刻炸）。
- **共享返回尾声不是依赖**：`0x2C93B`(`xor eax,eax`)/`0x2C93D`、`0x2BB2A`、`0x2C439`
  都是跳入的返回尾声；C 里只写 `return`，**严禁** `jmp`/`call` 机器码尾声。

---

## 63.3 逐函数语义（以机器码为准）

### 63.3.1 `0x2B996` / `fx_dots7`

只读表：`dir[7] @0x524EE`（7 字节，`movsd/movsw/movsb`）、`pos[7] @0x524F5`、
`row[7] @0x52511`；`record[+6]==0` 时 `pos[] += 148`。

- `selector 3`：写 **8** 个相位 `dword_53F76[i]=-2*i`（数组名义 7，但第 8 个落在
  `0x53F92` = `dword_53F92[0]`——原码真实行为，**不改循环上界**），返回 **28**。
- `selector 4`：每粒子先 `v==3 → svc_play_sfx(bank,1,1)`，再
  `0 <= v < 0x10 && dir[i]==1` 时 `res_blit(buf, v, dst+pos[i]+pitch*row[i], pitch, -1)`；
  返回 0。
- `selector 5`：`0 <= v < 0x10 && dir[i]==0` 时 blit；`++phase`，`==9` 置 `r=1`；返回 r。
- 其余返回 0。

### 63.3.2 `0x2BB33` / `fx_dots8`

只读表：`pos[8] @0x5252D`、`row[8] @0x5254D`；`record[+6]==0` 时 `pos[] += 148`。

- `selector 3`：8 个相位 `-2*i`，返回 **31**。
- `selector 4`：
  - 前 4 个（`i<4`）：`0 <= v < 0xF` 时 `res_blit(buf, v, dst+pos[i]+pitch*row[i]+80, …)`；
  - 后 4 个（`4<=i<8`）：index = `v+15`，目标用 **`(i+4)%8`** 的 `pos/row`，同样 `+80`。
- `selector 5`：
  - 前 4 个：index = `v+15`，目标用 `(i+4)%8`；后 4 个：index = `v`，目标用 `i`；
  - 逐粒子 `++phase`：`==9 → r=1`，`==5 → svc_play_sfx(bank,1,1)`；返回 r。
- 判定边界是 **`< 0xF`（≤0xE）**，不是 `<0x10`——这是 harness 第一跑抓到的真 bug
  （见 §63.5）。

### 63.3.3 `0x2BD6C` / `fx_blob` + `0x2BF83` / `fx_advance`

`v15 = (record[+6]==0) ? 1 : 0`；`selector` 分支：

- `0`：`byte_53FB2=byte_53FB3=byte_53FB4=0`，返回 **29**。
- `3`：`byte_53FB2=0x10`，返回 **12**。
- `6`：`svc_play_sfx(bank,3,1)`；`byte_53FB2=10`，返回 **10**。
- `1/7`：`v15!=0` → 返回 0；否则 `byte_53FB2==10 && sel==1 → =15`，
  `fx_advance(&byte_53FB2,&byte_53FB4,dst,pitch,buf)`，返回 0。
- `2/8`：`byte_53FB2==7 → svc_play_sfx(bank,1,1)`；`v15!=0` 时
  （`byte_53FB2==10 && sel==2 → =15`）`fx_advance(...)`；然后 `byte_53FB2==0x10` 时
  `res_blit(buf,0x10,dst,pitch,-1)`，返回 0。
- `4`：`v15==0` 时 `res_blit(buf,15,dst+1-pitch,pitch,-1)`，返回 0。
- `5`：`v15!=0` 时 `res_blit(buf,15,dst-1-pitch,pitch,-1)`；
  `res_blit(buf,byte_53FB2,dst+1-pitch,pitch,-1)`；`++byte_53FB2`；
  `==0x11 → svc_play_sfx(bank,2,1); return 1`；`==0x12 → byte_53FB2=0x10`；返回 0。

辅助 `fx_advance`（86 B）：

```c
RES_BLIT(buf, (int)*phase, dst, pitch, -1);
ret = *(const uint8_t *)(b + *(const int32_t *)(b + 4*(int)*phase + 8) + 6);
if ((uint8_t)(++*counter) == (uint8_t)ret) { *counter = 0; ++*phase; }
return ret;
```

`buf` **既是资源基址又是子偏移表基址**（`+8` 起 4×phase 的表，子块 `+6` 是帧数）。
harness 的 `g_buf` 就按这个结构铺（见 §63.4）。

### 63.3.4 `0x2BFD9` / `fx_dots12`

只读表：`v19[12] @0x5256D`（dword）、`v21 @0x5259D`(12 B)、`v20 @0x525A9`(12 B)；
`record[+6]==0` 时 `v19[] += 20`。

- `0`：`dword_53FB5[i]=-2*i`、`dword_53FE5[i]=i`；`byte_54015=12`、`byte_54016=0`、
  `byte_54017=0`；返回 **2**。
- `3` → **40**；`6`：`byte_54016=1` → **20**。
- `2/5/8`：先 `byte_54017=(uint8_t)((uint8_t)(byte_54017+1)%2)`（**先 8 位回绕**）；
  每粒子 `slot=dword_53FE5[i]`：`0 <= v < 0xB` 时
  `res_blit(buf, v+v20[slot], dst+v19[slot]-pitch*v21[slot], pitch, -1)`；
  **相位门 `byte_54017==0`** 时才推进：`v==0 && v20[slot]!=0 → svc_play_sfx(bank,2,1)`；
  `++phase`；`phase==3 → { if (v20[slot]==0) svc_play_sfx2(bank,1,1); r=1; }`；
  `phase==0xB && byte_54016==0` → `byte_54015=(byte_54015+1)%12`、槽取新值、相位清 0。
- **易错点**：`r=1` 是 `++phase==3` 的**无条件**结果；`svc_play_sfx2` 只在 `v20[slot]==0`
  时才响。规划稿 §3.4 把 `r=1` 写进了 `v20[slot]==0` 分支，harness 当场抓到（§63.5）。

### 63.3.5 `0x2C441` / `fx_dots6b`

只读表 `pos[10] @0x525DD`；`record[+6]==0` 时 `pos[] += 143`。

- `0`：`for i<6 { dword_54050[i]=-2*i; dword_54068[i]=i;
  byte_54080[i]=6*(rand%2); }`；`byte_54086=6`、`byte_54087=0`；返回 **1**。
- `3` → **12**；`6`：`byte_54087=1` → **8**。
- `2/5/8`：每粒子 `slot=dword_54068[i]`；`0 <= v < 6` 时
  `res_blit(buf, byte_54080[i]+v, dst+pos[slot], pitch, -1)`；
  `v==0` 时 `i==0 → svc_play_sfx(bank,1,1)`、`i==3 → svc_play_sfx2(bank,1,1)`；
  `++phase`；`==2 → r=1`；`==7 && byte_54087==0` →
  `byte_54086=(byte_54086+1)%10`、槽取新值、相位清 0、重掷方向；返回 r。
- `rand%2` 是**有符号 idiv**（`fx_rand_mod2` 保留符号，与已转 `fx_dots6` 同）。

---

## 63.4 对拍（`src/fxcheck.c`，扩展）

沿用既有骨架（`le_reserve_address_space` → `le_open` → `le_map_and_relocate` →
`install_hook` 5 字节 `jmp`；不碰 CRT/文件/AIL/时钟）。

- **记录桩**（不新增）：`0x2EB9F`/`0x25A96`/`0x25B45`/`0x4EBE3`，逐参记录事件。
- **环境合成**：`dword_53A45` → host scratch 80×4 记录表；`dword_54153=0xDEADBEEF`；
  粒子/槽/方向/轮转字节多组 profile，覆盖边界 `-1/0/1/2/3/4/5/6/7/8/9/10/11/0xF/0xFF/
  0x7FFFFFFF`；**dots12 槽保持在 0..11、dots6b 槽保持在 0..9**（原码不查边界，
  越界会读位图外的机器码世界，`PITFALLS` §8-71）。
- **`g_buf` 兼作 fx_advance 资源块**（2048 B）：`[8, 8+4*256)` 是 int32 子偏移表，
  子块 `+6` 放帧数 `p*7+3`；直测覆盖 `phase/step` 多组。
- **比较判据**：① 返回值全等；② 事件序列全部实参逐项相等；③ **整个 obj1**
  （`0x50000..0x556B0`）逐字节相等；④ 记录表 scratch 不变；fx_advance 另比
  `*phase`/`*counter` 副作用。
- **用例矩阵**：dots7/dots8/dots6b 各 7 profile、dots12 8、blob 16、advance 直测
  `37×24×3` ⇒ 新增 **8964 例**；合计 **12884 / 0**。

---

## 63.5 踩到的两个真 bug（否则会静默接错）

1. **`fx_dots8` 的 blit 上界是 `< 0xF` 不是 `< 0x10`**（第一跑 `FAIL fx_dots8 p5 sel=4
   flag=0 rec=0: event count 0/8`）。profile 全 `0xF`：原机器返回"不画"，C 画了 8 次。
   0x2BBE8/0x2BC36 都是 `cmp …,0Fh; jge skip`。
2. **`fx_dots12` 的 `r=1` 无条件**（`FAIL fx_dots12 p2 sel=2: return 1/0`）。规划稿把
   `r=1` 写进 `v20[slot]==0` 分支；机器码 0x2C193 `jnz 0x2C1A7` 是**跳过 sfx2 后仍执行**
   0x2C1A7 的 `mov [var_14],1`。修正后 12884/0。

两条都属"规划稿伪代码略去了分支细节、执行者必须回机器码定案"的范畴，判据全部由
`fxcheck` 的返回值 + 事件序列 + 全区快照钉死。

---

## 63.6 判据

| 项 | 结果 |
|---|---|
| `fxcheck`（扩） | **12884 / 0**（3920 → 12884，新增 8964） |
| `letest` | reference check OK, **exact match** |
| 既有 21 个 `*check` | 全部 0 failure：rlecheck 1900、gfxcheck 1450、sprite24check 2100、utilcheck 2200、pathcheck 1000、rescheck 160、tablescheck 4528、rle2check 1200、dlgcheck 800、boxcheck 240、keycheck 100、keyscheck 102 keys PASS、leafcheck 72008、fadecheck 4000、mapcheck 97000、scenecheck 1480、bgmcheck 6000、reccheck 37398、typecheck 1616、vmcheck 5512、doscheck 49/49 |
| `regress.ps1` | **8/8 PASS**、`repl: installed 104`、`FD2.TMP=207360` |
| A/B 同 tick `--shot-tick=500` | `none↔none2` **0 / 64000 px**、`none↔all` **0 / 64000 px** |
| Linux | `make -f Makefile.linux` 通过、`letest-linux` 三对象 **exact match**、`doscheck-linux` **49/49** |
| `translation_map` / `func_ranking` | **104 wired / 1359（7.7%）**（`--check` 通过） |

进度 **104 / 1359（7.7%）**。

---

## 63.7 下一步

`funcs_30469` 整表只剩 **`[6] 0x2C67D`（1151 B，CRT `cos/sin/__CHP`）**，单列一轮；
转完该表即闭合分派器 `sub_31266`（632 B）的依赖（还差 `0x2FB2C`(744)+`0x2FE14`(237)）。
表内其余共享尾跳（`0x2BB2A`/`0x2C439`/`0x2C93B/0x2C93D`）一律按语义 `return`，不复刻跳转布局。

---

## 63.8 备注：A/B 抓图要用既有 `build/ab_run.ps1`

手搓 `--screenshot/--shot-tick` 命令时**必须同时给 `--autokey` 并删掉 `FD2.TMP`**，
否则 `none↔none2` 基线就差 50%+（游戏启动路径随 `FD2.TMP` 是否存在/内容变化）。
既有 `build/ab_run.ps1` 已封装正确配方（`--autokey=…;--exit-when-file`，
与 `PITFALLS` §8-73 的 `--exit-after=60` 一致）；本轮用它得到 **0 px**。
