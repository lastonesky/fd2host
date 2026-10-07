# 轮次明细：角色记录表七个数据叶子（第 105–111 个转译函数）

第 28 轮（`rounds/28-rec-slots.md` §58.5）就排好队的下一批：**依赖全闭合的记录表叶子**，
全是"读/写 80 字节记录表的固定字节 + 返回"这类纯数据操作，没有文件 / 堆 / VGA / 时钟 / AIL。

| addr | size | callers | C 名 | 语义（以机器码为准） |
|---|---|---|---|---|
| `0x1B8A6` | 65 | 10 | `rec_slot_free(int)` | 八槽 state 字节 **bit7 清零**（即空闲）的个数 |
| `0x1B83D` | 105 | 8 | `rec_slot_find(int,int)` | 首个 state **bit6 置位** 且 value 字节按 `want_high` 分 `<0x80`/`>=0x80` 的槽，否则 `-1` |
| `0x1CA89` | 62 | 14 | `rec_sub_table5(int,int)` | `record[+68]`（**16 位字**）− 表 `0x619FD` 第 `tidx` 项的字节 5；返回**记录地址** |
| `0x13512` | 36 | 11 | `rec_flag_or80(int)` | `record[+5] \|= 0x80`；返回 **`80*index`（偏移，不是地址）** |
| `0x32975` | 36 | 8 | `rec_flag_set1(int)` | `record[+5] = 1`；返回 **`80*index`** |
| `0x34D64` | 46 | 10 | `rec_status_mask_records(void)` | 记录 **10..27** 共 18 条的 `record[+52] &= 0x80`；返回**表基址** |
| `0x35009` | 25 | 9 | `rec_status_set_record14(void)` | 记录 **14** 的 `record[+52] = 0x83`；返回 **`base+1120`** |

合计 **375 B**、69 个到达点。接入既有分组 **`REPL_REC`**（不新增组），模块沿用
`src/game/rec.c/.h`，对拍沿用 `src/reccheck.c`（不新建 target）。

`reccheck` **37398 → 42225 / 0**（新增 4827 例）；既有 22 个 `*check` 全部维持通过；
`regress` **8/8**、`repl: installed 104 → 111`、`FD2.TMP=207360`；静态帧 `--shot-tick=500`
A/B `none↔none2` 与 `none↔all` 均 **0 / 64000 px**；Linux 构建 + `letest` exact match +
`doscheck` **49/49**；`translation_map` **111 / 1359（8.2%）**。

---

## 64.1 为什么是这一批

1. **第 28 轮明确排队的候选**（`rounds/28` §58.5、`PROGRESS` §4 第 1 项"下一批同族候选"）。
2. **依赖全闭合**：只读写游戏数据段全局（`dword_53A45` 记录表）+ 一个已接入的表访问器
   （`0x1CA89` 调 `0x4E866`/`tbl_619FD`，第 27 轮起在 `tables.c`）。零新缝。
3. **不碰 `0x205BE`**（同族候选之一）：`funcs_1197B[0]` 等调用者调的是 **`0x205B4`**，
   `0x205BE` 是其**函数体内被单独 xref 的尾块**，不是真入口。按条目点 patch 会落到
   函数中间 ⇒ 排除，留待连同 `0x205B4` 本体一起做（`repl` 条目只允许真入口，
   见 `docs/TRANSLATION.md` §3）。
4. **顺带收两张分派表**：`funcs_1199C`(`0x51B91`) 的 `[28]=0x34D64`、`[36]=0x35009`
   落到本轮；两者同时还有 10 处直接 `call`，patch 一个入口覆盖全部到达点。

---

## 64.2 ABI 与陷阱（先读）

七个函数都是 **cdecl 栈参**；Hex-Rays 印的 `__fastcall(a1..a4, a5)` 是
`push <帧>; call 0x3702F`（Watcom `_chkstk` 栈探针）的伪像，`a1..a4` 是垃圾
（`rounds/08` §37.3、`rounds/33` §63.2、`PITFALLS` §8-75）。**真实形参只有 `arg_0`（以及
`0x1B83D`/`0x1CA89` 的 `arg_4`）**，实参序按 `push` 顺序定案。

### 64.2.1 `0x34D64`/`0x35009` 读不到任何实参

分派点（`sub_117E7` 内，两处）：

```asm
1199B:  push   esi
1199C:  call   funcs_1199C[eax*4]
119A3:  add    esp, 4          ; 调用者清栈
```

即**压 1 个实参、调用者清栈**，而被选中的 `0x34D64`/`0x35009` 的机器码
**一个 `arg_0` 都不读**（反汇编见下）。所以 C 写成 `void (void)` 是精确的——
不需要在签名里留一个没用的参数，也不需要 harness 去维护那个参数。

### 64.2.2 返回值各不相同，必须逐个钉死

这是本轮最容易接错的地方——**六个返回值语义里只有一个是"记录地址"**：

| addr | EAX 在 `retn` 时是什么 | 反汇编证据 |
|---|---|---|
| `0x1CA89` | **记录地址** `base + 80*index` | `add eax,ebx` 后 `retn`，`eax = dword_53A45 + 80i` |
| `0x13512` | **偏移** `80*index`（**不含基址**） | `shl eax,4` 得到 `80i`，`or byte ptr [edx+eax+5],80h`，`retn` 前 EAX 未再改 |
| `0x32975` | **偏移** `80*index` | 同上，`mov byte ptr [edx+eax+5],1` |
| `0x34D64` | **表基址** `dword_53A45` | 循环体里 `mov eax,dword_53A45`，`cmp edx,12h; jl` 退出时 EAX 保持该值 |
| `0x35009` | **`base + 1120`**（记录 14 的地址） | `mov eax,dword_53A45; add eax,460h; mov byte ptr [eax+34h],83h; retn` |
| `0x1B8A6` | 计数 `0..8` | `mov eax,ebx` |
| `0x1B83D` | 槽号 `0..7` 或 `-1` | `mov eax,edx` / `jge → mov eax,-1` |

> `0x13512`/`0x32975` 返回**偏移**这件事是规划稿没写的：两个函数的 decompile 视图都是
> `return 80 * a5;`，**`dword_53A45` 只被用来算地址，不在返回路径上**。若照直觉写成
> 返回记录地址，调用方拿 EAX 做后续算术会整体错位 `base`（约 5.2 MB），而且
> `reccheck` 的返回值比较立刻会炸——所以它属于"harness 必须逐个比返回值"的价值。

### 64.2.3 `0x1CA89` 是 16 位字算术

```asm
movzx   ebx, word ptr [eax+44h]   ; +68 = 0x44
movzx   edx, byte ptr [edx+5]
sub     ebx, edx                  ; 32 位寄存器里的减法
mov     [eax+44h], bx             ; 但只写回低 16 位
```

`sub` 在 32 位寄存器里做、**写回只取低 16 位** ⇒ 等价于 **`uint16_t` 回绕**。
C 必须显式 `uint16_t` 中转，不能写成 `rec[68] -= x`（那是 8 位，会丢掉字节 69）
也不能写成 32 位减法（借位不进位 16 位结果）。harness 用 5 组构造值专门打回绕
（`0x0003-0x0004 → 0xFFFF`、`0x0000-0x0001 → 0xFFFF`、`0xFFFF-0x0001 → 0xFFFE`…）。

### 64.2.4 `0x34D64` 的窗口是**常量**，不是 `dword_53BEB`

循环写的是 `lea ebx,[edx+0Ah]` ⇒ 记录 `edx+10`，`cmp edx,12h` ⇒ 18 次 ⇒
**记录 10..27**，与 `dword_53BEB`（记录数）**无关**。harness 构造了
记录 9 / 10 / 27 / 28 四个边界：9 和 28 必须保持原样（**窗口外不动**），
10 和 27 必须被 `&= 0x80`。

---

## 64.3 对拍（`src/reccheck.c`，扩展）

沿用既有骨架（`le_reserve_address_space` → `le_open` → `le_map_and_relocate` →
`install_hook`；不碰 CRT/文件/AIL/时钟）。这七个函数全是纯数据操作，**没有服务桩、
没有事件序列**，比较判据只有两条：

1. **返回值全等**（`cmp_read`）——包括 `-1`、槽号、计数、以及 §64.2.2 那 5 个不同语义；
2. **记录表逐字节相等**（`cmp_mem`）——写函数用 `obuf`/`cbuf` 两份同种子副本，
   指针返回值归一化成 `ptr - obuf` / `ptr - cbuf` 再比（否则两份副本的基址不同，
   比较必然假失败）。

**读函数不需要双副本**：`G53A45` 直指 `inbuf`，两边读同一片字节，只比返回值。

**`0x619FD` 表的处理**：它是**真实游戏数据地址**（obj2，`0x60000..0x634D2` 内），
两边读到的都是同一片已映射字节；harness 只把用到的 200 项（`0x619FD..0x62069`，
安全地落在数据段里）填成受控随机值，`rec_sub_table5` 的 C 侧经
`tbl_ptr(TBL_619FD, 7, tidx, 0)` 得到与原 `sub_4E866` 完全相同的指针
（`tables.c` 里 `rep_t619FD` 就是这一行）。

**环境合成**：`dword_53A45` → host scratch 80×MAXREC 记录表；槽位 state/value
用 `leaf_slots()` 铺 5 种 profile（无 bit6 / bit6 随机 / 全 `<0x80` / 全 `>=0x80` /
序号相关的确定值），边界字节 `0x7F`/`0x80`/`0x00`/`0x01`/`0xFF` 全覆盖。

**用例矩阵**：

| 测试 | 随机 | 构造 | 小计 |
|---|---|---|---|
| `test_free` | 800 | 3（全空 / 全满 / 交替） | 803 |
| `test_findslot` | 800 | 10（5 profile × 2 个 `want_high`） | 810 |
| `test_sub5` | 800 | 5（16 位回绕） | 805 |
| `test_flagwriters` | 800×2 | 5（`+5` ∈ 00/01/7F/80/FF） | 1605 |
| `test_status_records` | 400×2 | 4（窗口边界 9/10/27/28） | 804 |
| 合计 | | | **4827** |

---

## 64.4 顺带修的两个工具 bug

1. **`repl_parse()` 少认一个组名 `map`**（`PITFALLS` §8-77）：`--replace=map` 的未知 token
   被静默丢弃 ⇒ `mask=0` ⇒ **行为等同 `--replace=none`**。这会让"按组做 A/B"这类
   对照实验**静默地拿基线当对照组**，比报错更危险。修法是补 `else if (!plat_stricmp(tok,
   "map")) mask |= REPL_MAP;`，实证：修前 `--replace=map` 打出 `installed 0`，
   修后 `installed 5 (mask 0x2000)`。`gfx`/`rec` 等其余 14 个组名都在，只有这一个漏。
2. **新增跨模块调用只会在链接期暴露**（`PITFALLS` §8-78）：`rec.c` 现在引用 `tbl_ptr`，
   而 `dlgcheck`/`boxcheck`/`keycheck`/`leafcheck`/`typecheck` 这 5 个 target 原本
   `srcs` 里**只有 `rec.c` 没有 `tables.c`** ⇒ `LNK2019: 无法解析的外部符号 _tbl_ptr`。
   编译一个 warning 都没有（`.c` 内是 extern），必须靠"把所有 check target 编一遍"才抓到。
   修法是给这 5 个 target 补 `"game\\tables.c"`（与 `mapcheck`/`reccheck` 已有的一致）。

---

## 64.5 判据

| 项 | 结果 |
|---|---|
| `reccheck`（扩） | **42225 / 0**（37398 → 42225，新增 4827） |
| `letest` | reference check OK, **exact match** |
| 既有 21 个 `*check` | 全部 0 failure：rlecheck 1900、gfxcheck 1450、sprite24check 2100、utilcheck 2200、pathcheck 1000、rescheck 160、tablescheck 4528、rle2check 1200、dlgcheck 800、boxcheck 240、keycheck 100、keyscheck 102 keys PASS、leafcheck 72008、fadecheck 4000、mapcheck 97000、scenecheck 1480、bgmcheck 6000、fxcheck 12884、typecheck 1616、vmcheck 5512、doscheck 49/49 |
| `regress.ps1` | **8/8 PASS**、`repl: installed 111`、`FD2.TMP=207360` |
| A/B 同 tick `--shot-tick=500` | `none↔none2` **0 / 64000 px**、`none↔all` **0 / 64000 px** |
| 分组开关实证 | `--replace=map` 修前 `installed 0` → 修后 **`installed 5 (mask 0x2000)`** |
| Linux | `make -f Makefile.linux` 通过（`rec.c` 单独 `-Wall -Wextra` **0 warning**）、`letest-linux` 三对象 **exact match**、`doscheck-linux` **49/49** |
| `translation_map` | **111 wired / 1359（8.2%）**（`--check` 通过） |

进度 **111 / 1359（8.2%）**。

---

## 64.6 下一步

记录表侧还剩的依赖闭合叶子不多，按 `re/func_ranking.csv` 继续收；
更值钱的下一步是**继续分派表**：

1. `funcs_30469` 只剩 `[6] 0x2C67D`（1151 B，含 CRT `cos/sin`，另开一轮），
   转完即可让小分派器 `sub_31266`（632 B）只差 `0x2FB2C`(744)+`0x2FE14`(237) 即闭合
   （`rounds/33` §63.7）。
2. `funcs_1199C`(`0x51B91`) 本轮收了 `[28]`/`[36]`，表里还有 `[0]=0x34531` 起的大批
   记录表服务（`0x34531`/`0x3460B`/`0x34673`/…），多为同族 80 字节记录操作——
   **下一批主候选**。
3. 被本轮排除的 `0x205BE`：连同其真入口 `0x205B4`（`funcs_1197B` 里出现 14 次的
   默认项）一起评估，看是否依赖闭合。

---

## 64.7 备注

- A/B 沿用第 33 轮 `ab_run.ps1` 的既有配方（`--autokey=…` + `--exit-when-file`，
  必须删 `FD2.TMP`，否则基线自己就差一半，`rounds/33` §63.8、`PITFALLS` §8-73）。
- `translation_map.py` 的 `MODULE_INFO["rec.c"].cases` 从 `36327` 改到 `42225`
  （该键被 `rec.c` 与 `unit.c` 共用，本轮一并更正），再跑生成 + `--check`。
