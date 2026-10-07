# 轮次明细：`funcs_30469` 效果动画 handler 四件套（第 96–99 个转译函数）

§62 按规划 agent 的分配，把 `funcs_30469 @0x524C6`（10 项函数指针表）里 **4 个依赖完全
闭合**的 handler 还原成 C：表项 `[4]/[7]/[8]/[9]` = `0x2C217`/`0x2CAFC`/`0x2CCF4`/`0x2CE1A`，
合计 **1630 B obj0 机器码 / 60 个到达点（4 handler × 15）**。它们只读写游戏数据段全局、
`memcpy` 三张只读偏移表（原址读取），并调 3 个**已接入**服务
（`res_blit` / `svc_play_sfx` / `svc_play_sfx2` / `util_rand`）；余下依赖只有 Watcom 栈探针
`0x3702F`（CRT，约定不转译）。新增模块 `src/game/fx.c` / `fx.h` + 对拍 `src/fxcheck.c`，
接入**新分组 `REPL_FX`**。

`fxcheck` **3920/0**（新）；既有 18 个 `*check` 全部维持通过；`regress` **8/8**、
`repl: installed 95 → 99`、静态帧 `--shot-tick=500` A/B **0 px**；Linux 构建 + `letest`
三对象 exact match + `doscheck` 49/49。

---

## 62.1 这一批

| addr | size | usage | 表项 | C 名 | 语义 |
|---|---|---|---|---|---|
| `0x2C217` | 554 | 15 | `funcs_30469[4]` | `fx_dots6` | 6 粒子；`[+6]==0` 时偏移表整体 +143 |
| `0x2CAFC` | 504 | 15 | `funcs_30469[7]` | `fx_dots3` | 3 粒子；`[+6]==0` 时偏移表整体 +130 |
| `0x2CCF4` | 294 | 15 | `funcs_30469[8]` | `fx_dots16` | 16 粒子；16 字节方向表 |
| `0x2CE1A` | 278 | 15 | `funcs_30469[9]` | `fx_toggle` | 双帧翻转 + 索引推进；无粒子数组 |

- **`callers_game = 14` 是 14 个间接分派点，不是 14 个函数**：全部长
  `call funcs_30469[ebp*4]`（基址 `0x524C6`，index 运行期），分布在
  `sub_2FF01`（10 处）与 `sub_31266`（4 处）。`data_xrefs = 1` = 表首指针。
  表实测（IDA 读 `0x524C6+4*i`）：

  | i | 0 | 1 | 2 | 3 | **4** | 5 | 6 | **7** | **8** | **9** |
  |---|---|---|---|---|---|---|---|---|---|---|
  | 目标 | `2B996` | `2BB33` | `2BD6C` | `2BFD9` | **`2C217`** | `2C441` | `2C67D` | **`2CAFC`** | **`2CCF4`** | **`2CE1A`** |

  入口 patch 后这 15 个到达点全部进 C（函数体内无外部跳入，逐全区 `XrefsTo` 已核）。

### 62.1.1 真实 ABI（4 个一致）

Hex-Rays 印成 `__fastcall … (a1..a9)`，那是 `push <帧大小>; call 0x3702F` Watcom 栈探针
（`_chkstk`）的伪像：探针吞掉 EAX 后，前 4 个"寄存器参数"是垃圾。**真签名 = cdecl 5 栈参**：

```c
int f(int rec, const void *buf, void *dst, int pitch, int selector);
```

判据（调用点 + `res_blit` 实参对齐，逐条 `--dump`）：

- 分派点 `0x30469`：`push 0; push 140h; push esi; push [var_2C]; push [arg_0]`，`add esp,14h`
  ⇒ 栈低到高 `arg_0=rec, arg_4=buf, arg_8=dst, arg_C=pitch, arg_10=selector`（`pitch=320`）。
- handler 内的 `res_blit` 序列 `push -1; push [arg_C]; push [arg_8]; push <index>; push [arg_4]`
  ⇒ `res_blit(buf, index, dst, pitch, -1)`，与 `res.h` 的
  `res_blit(buf,index,dst,pitch,mode)` 完全对齐。
- `selector` 只取低字节（`movzx eax, [esp+arg_10]` 是 byte 载入）。

---

## 62.2 逐函数语义与易错点

### 62.2.1 `0x2C217` / 554 B / `fx_dots6`（表项 `[4]`）

6 粒子 `dword_54018[6]`（相位）+ `dword_54030[6]`（0..9 槽）+ `byte_54048[6]`（方向，
`7*(rand%2)`）+ 轮转 `byte_5404E/4F`；10 dword 只读表 `unk_525B5`，`record[+6]==0` 时整体 +143。

- `selector 0`：播种 6 粒子并 `byte_5404E = 6`，返回 2。
- `selector 2/5/8`：每粒子 `0 <= v < 7` 时 `res_blit(buf, byte_54048[i]+v, dst+pos[d30[i]], pitch, -1)`
  —— **`res_blit` 的第 2 参是方向字节 + 相位，目的地偏移在 `dst` 上**（不是把相位当 index、
  偏移表当 index；规划稿 §3.4 此处语焉不详，以本条机器码为准）。`v==0` 时 `svc_play_sfx(bank,1,1)`；
  `++dword_54018[i]==3` 置 `r=1`；`==8 && byte_5404F==0` 时槽 `(byte_5404E+1)%10`、相位清零、
  重掷方向。
- `selector 3` 返回 12；`selector 6` 置 `byte_5404F=1` 返回 8；default 返回 0。
- **易错点**：`(byte_5404E+1)%10` 必须**先做 8 位回绕**（原码 `inc byte` 后再 `movzx; idiv 10`）
  —— 直接 `(byte_5404E+1)%10` 在 255+1 时给 6 而非 0；`rand%2` 是**有符号 idiv**。

### 62.2.2 `0x2CAFC` / 504 B / `fx_dots3`（表项 `[7]`）

3 粒子 `dword_540CB[3]` + 槽 `dword_540DB[3]` + `byte_540EB/EC/ED`；10 dword 表 `unk_5261E`，
`record[+6]==0` 时整体 +130。

- `selector 0`：**循环写 4 个元素**（`j=0..3`）——两数组各占地四 dword
  （`0x540CB`→`0x540DB` = `0x10`），`dword_540CB[3]`=`0x540D7`、`dword_540DB[3]`=`0x540E7`
  都在各自第四格里（不越界到下一变量）；`byte_540EB = 4`，返回 2。
- `selector 2/5/8`：先 `byte_540ED = (byte_540ED+1)%2`（同样先 8 位回绕）。每粒子
  `0 <= v < 5` 时 blit（index = v，目的地 +`pos[dword_540DB[k]]`）；**相位门 `byte_540ED==0`
  时才做采样与推进**：`v==1` 且 `k==0` → `svc_play_sfx(bank,1,1)`、`k==1` → `svc_play_sfx2(bank,1,1)`、
  `k==2` 无声；`++dword_540CB[k]==2` 置 `r=1`；`==7 && byte_540EC==0` 时槽 `(byte_540EB+1)%10`、
  相位清零。奇数相位**只 blit、不推进**。
- `selector 3` 返回 32；`selector 6` 置 `byte_540EC=1` 返回 16；default 0。

### 62.2.3 `0x2CCF4` / 294 B / `fx_dots16`（表项 `[8]`）

16 粒子 `dword_540EE[16]`；16 字节只读表 `unk_52646`（字节值 0/8/16/24 循环）。

- `selector 0`：`dword_540EE[j] = -2*j`，返回 3。
- `selector 2/5`：每粒子 `(uint32_t)v < 8` 时 `res_blit(buf, tbl[j]+v, dst, pitch, -1)`
  （index = 零扩展字节 + 相位）；`v==0` → `svc_play_sfx(bank,1,1)`，`v==4` → `svc_play_sfx2(bank,2,1)`；
  `++dword_540EE[j]==4` 置 `r=1`。
- `selector 3` 返回 34；`selector 6` 返回 2；default 0。
- `rec` 不使用。

### 62.2.4 `0x2CE1A` / 278 B / `fx_toggle`（表项 `[9]`）

无粒子数组，只 `byte_5412E`（相位）+ `byte_5412F`（翻转位）。

- `selector 0`：`byte_5412F=0; byte_5412E=1`，返回 20。
- `selector 1/7`：`byte_5412F==0` 时 `res_blit(buf, 0, dst, pitch, -1)`；`byte_5412F ^= 1`；返回 0。
- `selector 3` 返回 60；**`selector 4`：`res_blit(buf, 0, dst, pitch, -1)`，返回 0**。
- `selector 5`：`res_blit(buf, byte_5412E>>1, dst, pitch, -1)`（无符号除 2）；`byte_5412E==6`
  → `svc_play_sfx(bank,1,1)`、`==36` → `svc_play_sfx2(bank,2,1)`；`++byte_5412E` 后返回
  `(byte_5412E > 0x10 && byte_5412E < 0x2C)`（8 位比较）。
- `selector 6` 返回 20；default 0。

> **规划稿 §3.1 的 `case 4` 写成 `RES_BLIT(buf, 4, …)` 是错的**：机器码是 `push 0`。
> 见 §62.4 / `docs/PITFALLS.md` §8-75。

---

## 62.3 对拍（`src/fxcheck.c`）

沿用 `scenecheck` 骨架（`le_reserve_address_space` → `le_open` → `le_map_and_relocate` →
`install_hook` 写 5 字节 `jmp`；**不用 CRT 重定向、不用文件、不用 AIL/时钟**）。

- **记录桩**：`0x2EB9F` `res_blit(buf,index,dst,pitch,mode)`、`0x25A96` `svc_play_sfx(bank,index,loops)`、
  `0x25B45` `svc_play_sfx2(...)`、`0x4EBE3` `util_rand()`（返回一个**含负数 32 位**的固定池，
  两边共享同一序列，钉死 `sar edx,1Fh; idiv 2` 的符号语义）。
- **环境合成**：`dword_53A45` 指向一个 host scratch（记录表）作为 `dword_53A45 + 80*rec`；
  `dword_54153` 设哨兵 `0xDEADBEEF`；粒子数组/槽/相位用 6–9 组 profile（含边界
  `0/3/4/7/8/0x7FFFFFFF/-1`、`byte_5412E ∈ {5,6,35,36,43,0x10,0x2B}`、`byte_5412F ∈ {0,1}`）。
  **槽下标一律保持在 0..9**（原码不做边界检查，越界会读位图外的机器码世界，见 §8-71）。
- **比较判据**：① 返回值 int 全等；② 事件序列的**全部实参**逐项相等（次数 + 顺序）；
  ③ **整个 obj1 数据段**（`0x50000..0x556B0`）逐字节相等（证明 C 只动它该动的全局）；
  ④ 记录表 scratch 逐字节不变。
- **用例矩阵**：4 handler ×（6/6/7/9 profile）× 14 selector（`0..9,0x100,0xFF,0x12,0x1FF`）
  × 2 记录分支 × 5 rep = **3920 例**。
- 构建目标 `fxcheck`（`le.c` + `game/fx.c`，`/BASE:0x60000000`）。

**当场抓到 1 个真 bug**（否则会静默接错）：`fx_toggle` `case 4` 的 index 应为 0 而非 4
（见 §62.4）。修正后 **3920/0**。

---

## 62.4 踩坑：栈探针伪像把立即数实参藏进"寄存器参数"

`fx_toggle` 的 `case 4` 在 IDA 的 9 参视图里是
`sub_2EB9F(4, v9, a3, v10, a6, 0, a7, a8, -1)`——第一个 `4` 是探针吞掉的 EAX 位置，真正的
`res_blit` index 是它后面那个 `0`（`0x2CE9B push 0`）。规划 agent 的伪代码照抄了 9 参视图的
第一个参数，于是 `case 4` 被写成 `RES_BLIT(buf, 4, …)`；`fxcheck` 第一跑就报
`event 0 blit arg1 0/4`。

**教训**：栈探针污染的 `__fastcall` 视图里，**实参既会错位也会出现"假立即数"**；
定参必须回到 `--dump` 的 `push` 顺序，逐条核对，不信 Hex-Rays 的第一参数。
详见 `docs/PITFALLS.md` §8-75。

---

## 62.5 判据

| 项 | 结果 |
|---|---|
| `fxcheck`（新） | **3920 / 0** |
| 既有 18 个 `*check` | 全部维持通过（rlecheck 1900、gfxcheck 1450、sprite24check 2100、utilcheck 2200、pathcheck 1000、rescheck 160、tablescheck 4528、rle2check 1200、dlgcheck 800、boxcheck 240、keycheck 100、reccheck 37398、typecheck 1616、vmcheck 5512、scenecheck 1480、mapcheck 97000、leafcheck 72008、fadecheck 4000，均 0 failure） |
| `regress.ps1` | **8/8 PASS**、`repl: installed 99`、`FD2.TMP=207360` |
| A/B 同 tick `--shot-tick=500` | `none↔none2` **0 / 64000 px**、`none↔all` **0 / 64000 px** |
| Linux | `make -f Makefile.linux` 通过、`letest-linux` 三对象 **exact match**、`doscheck-linux` **49/49** |
| `translation_map` / `func_ranking` | **99 wired / 1359（7.3%）** |

进度 **99 / 1359（7.3%）**。

---

## 62.6 下一步

`funcs_30469` 整表还剩 6 个 handler + 1 个 86 B 辅助 `0x2BF83`：
`0x2B996`(413)、`0x2BB33`(569)、`0x2BD6C`(535)+`0x2BF83`、`0x2BFD9`(574)、`0x2C441`(572)、
`0x2C67D`(1151，仅 CRT 浮点依赖)。转完即可让唯一的小分派器 `sub_31266`（632 B）整函数依赖闭合。
其中 `0x2B996/0x2BB33/0x2C441` 有共享尾跳（Hex `JUMPOUT`），按语义写、不复刻跳转布局
（与 `scene.c` 共享尾 `0x237C8` 同处理）。
