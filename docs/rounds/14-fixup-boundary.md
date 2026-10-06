# 轮次明细：fixup 跨页两类之辨 —— 补回 11 处漏掉的重定位
§44 用户开了 Ghidra 桥之后的比对，推翻了 §43 里"差异已解释"的判断，把加载器修到
**与 Ghidra 逐字节完全一致**（原判据恢复）。
---

## 44.1 起因：三家一对比，"已解释"就不成立了

§43 的判据是两条：跨平台哈希一致（可移植性）+ 与 IDA 导出的参考镜像逐字节（正确性）。
后者当时给出 `obj0: 11/257833 bytes differ`，我把它归进两类"已知差异"（页边界 / BSS 尾）
**并通过了**。用户开启 Ghidra 桥后导出第三份镜像：

| 对象 | Ghidra | 我们（改前） | IDA |
|---|---|---|---|
| obj0 | `0x2C48FADEDD2A735B` | `0xCB737A9DC0F6653E` ✗ | `0x2C48FADEDD2A735B`（= Ghidra） |
| obj1 | `0x3C879E6011769348` | `0x3C879E6011769348` ✓ | `0x0519…`（BSS 尾填 FF） |
| obj2 | `0xB45FE50C6829E13B` | `0xB45FE50C6829E13B` ✓ | 同 ✓ |

**两家独立反汇编器在 obj0 上一致、与我们不一致** ⇒ 我那句"页边界差异是已知的"是错的。
（教训：不要先给差异安个理由再放过它；第三家一比就露馅。）

## 44.2 深挖：`tools/fixup_dump.py` 把 22 条被跳的记录分成两类

按 `le.c` 的语法走 fixup 表（`tools/fixup_dump.py`，可按地址查任意一条记录），
`le.c` 当时跳过的 22 条**不是**一类东西：

| 类 | 数量 | 特征 | 正确做法 |
|---|---|---|---|
| **合法跨页操作数** | 11 | `src ∈ [0xFFD, 0xFFF]`：操作数**起始于本页**、尾巴伸进下一页（如 page1 `src=0xFFF` → 写 `0x10FFF..0x11002`，目标 `0x53BEB`） | **必须写**。同对象的页在我们的映射里是连续的，写进去正好落在 Ghidra/IDA 的位置 |
| **越界源** | 11 | `src > 0xFFF`（`0xFFFE`/`0xFFFF`）：源偏移根本不在本页（page2 `src=0xFFFF` → `0x20FFF`，已越出 page2 的 `0x11000..0x11FFF`） | **必须跳**。Ghidra/IDA 同样不写（那些地址上没有差异）；写它会**覆盖无关代码** |

**`§8-11` 只对了一半**：当年"跳过跨页记录，否则崩在 0x3E000"——崩溃来自**越界源**那一半，
而 11 条合法跨页被一起跳掉，留下 11 个**没被重定位的指针**（文件里的占位字节
`00/02/03` vs 目标值 `05/03/04`），这正是两家反汇编器与我们不一致的地方。

`le.c` 的新规则（`0x07` 分支）：

```c
if (srcoff >= LE_PAGE_SIZE)            { bad_source++; skip; }   /* 源不在本页 */
if (srcoff + 4 > LE_PAGE_SIZE)         /* 跨页：仅当下一页属于同一对象才写 */
    if (下一页不在本对象内)            { cross_page++; skip; }
```

## 44.3 判据

| 项 | 结果 |
|---|---|
| fixup 统计 | `applied=7937 → **7948**`（+11 = 补回的合法跨页）、`out-of-page sources=11`、`boundary writes refused=0`、`leftover=0`、`bad=0` |
| **vs Ghidra（原判据）** | obj0/obj1/obj2 **全部 `0 bytes differ  <-- exact match`**，`reference check OK, exact match`，退出码 0 |
| 跨平台 | Windows 与 Linux `make -f Makefile.linux` 输出**逐字相同**（哈希 `0x2C48…`/`0x3C87…`/`0xB45F…`） |
| 游戏行为 | `regress.ps1` **ALL PASS（8/8）**、`FD2.TMP=207360`；同 tick（600）抓帧 vs 改动前 **0 / 64000 px** |
| 工具 | 新增 `tools/ghidra_objects.py`（从桥重导参考镜像，参考文件是 gitignored 的）、`tools/fixup_dump.py`（按地址查 fixup 记录） |

## 44.4 对判据体系的修正

- `letest` 的"两类已知差异"**保留但收窄用途**：它只对 **IDA** 的参考有意义（IDA 的 BSS 尾填 `FF`）；
  **对 Ghidra 现在是 0 差异**，所以 `reference check OK, exact match` 是常态，出现 `explained only`
  就意味着又产生了真实分歧，应当按本轮的方法用第三家复核。
- `PITFALLS` §8-11 的原话过窄 ⇒ 新增 §8-60 记录完整规则（两分法 + 同对象才可跨）。
- `AGENTS.md` 的"参考镜像从 Ghidra 导出"现在有了可执行工具：`python tools/ghidra_objects.py`。

## 44.5 下轮入口

跨平台继续：**第 2 刀 `dos.c`**（VEH / 文件服务 / 低内存镜像），见 `rounds/13-portability.md` §43.5。
