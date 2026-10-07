# 轮次明细：数字渲染链（第 74–76 个转译函数）
§55 继续沿 `0x11CAC` 的依赖拓扑收：`0x187D6`（数字渲染）、`0x1875D`（带索引选择的包装）、
`0x1AEB1`（带正负号的两位数）。三者只碰已转译的 `res_blit6`/`rle_decode` 与 CRT `sprintf`/`abs`，
闭合。`mapcheck` 扩到 **75500/0**。
---

## 55.1 这一批

| addr | 用法 | 转译 | 语义 |
|---|---|---|---|
| `0x187D6` | 41 | `dlg_draw_number(dst,pitch,value,base,digits)` | `value<0→0`；`digits==3 && value>999` → 画 `base+10` 精灵；`digits==2 && value>99` → 画 `0x5D`；否则 `sprintf(buf,"%0.<digits>d",value)` 后逐位画 `res_blit6(dst+6*i, …, base + buf[i]-'0')` |
| `0x1875D` | 7 | `dlg_draw_number_pair(dst,pitch,value,compare,digits)` | 索引 = `(value==compare)?0x1F:0x2A`，其余转给上者 |
| `0x1AEB1` | 2 | `dlg_draw_number_signed(dst,pitch,value)` | 负数取 `abs` 并选 `0x84`、否则 `0x83`，先 `rle_decode` 该精灵，再在 `dst+8` 画两位数（base 31） |

放在 `dlg.c`（`dword_53A81` 就是它已在用的框体资源），`dlg.h` 声明。

## 55.2 对拍

`mapcheck` 里合成一个 256 个子图的 `*(0x53A81)`（每个 `u16 8,8` + 8 行全字面 token，任何数字索引都能解析），
然后原机器码 vs C 逐字节比整幅目标缓冲：

| 函数 | 用例 |
|---|---|
| `dlg_draw_number` | 随机 `value`/`base`/`digits`（含触发 `>999`/`>99` 特殊精灵的分支） |
| `dlg_draw_number_pair` | 命中/不命中 `compare` 两类 |
| `dlg_draw_number_signed` | 正/负值（走 `abs` 与两个符号精灵） |

结果 **75500 cases, 0 failures**；`leafcheck` 72008/0（dlg.c 变动后回归）。
整套：`regress` **8/8**、`repl: installed 76`、A/B 同 tick **0 px**、Linux `make`+`letest`+`doscheck` 全过。
进度 **76 / 1359（5.6%）**。

> 链接连带：`dlg.c` 现在会引用 `res_blit6`/`rle_decode`/`rec_flag`，所以所有链 `dlg.c` 的
> 检查目标（dlgcheck/boxcheck/keycheck/typecheck/leafcheck）都补上了 `game\res.c`、`game\rle.c`、
> `game\rec.c`。**改共享模块时要检查所有链接它的 harness**（这类错误是链接期才炸，很容易漏）。

## 55.3 下一步（`0x11CAC` 依赖剩余）

`0x129EC`(218，差 `0x12AC6`/`0x1F183`)、`0x12AC6`、`0x1F183`(73) → `0x127A9`(55)→ 之后
`0x122DC`(1051，只差已转译的 `map_blit_tile`)、`0x24D22`、`0x4E31C`(101) → `0x11EEE`(885) →
`0x11CAC`(84) → `0x135DD`(98) → `0x1366A`(110)。
