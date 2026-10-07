# 轮次明细：角色记录「8 槽」字段访问器簇（第 82–85 个转译函数）

§58 按规划 agent 的分配，收 `src/game/rec.c` 里角色记录 8 槽字段访问器的四个叶子：
`0x1B722` / `0x344F2` / `0x1BB8C` / `0x1B8E7`。四个都是纯数据、无堆/无 VGA/无 I/O 的
cdecl 叶子，扩进既有 `reccheck`，接入既有 `REPL_REC` 分组。
`reccheck` **28739 → 32010/0**；`regress` **8/8**、`repl: installed 85`、静态帧 A/B **0 px**。

---

## 58.1 这一批

| addr | usage | 转译 | 语义 |
|---|---|---|---|
| `0x1B722` | 20 | `rec_field_byte(index, slot)` | 记录 +11 + 2·slot 的**值**字节（8 个 2 字节槽在 +10：`[state, value]`），`movzx` 零扩展成 int |
| `0x344F2` | 20 | `rec_status_set(start, end, value)` | 闭区间 `[start,end]`：`rec[52] = (rec[52] & 0xF0) \| value`；`jle` 是有符号比较 |
| `0x1BB8C` | 13 | `rec_slot_claim(index, value)` | 占第一条空槽（state 字节 bit7 = 空）：清 bit7 + 存 value，返回 1；满则 -1 |
| `0x1B8E7` | 13 | `rec_slot_remove(index, slot)` | 左移删除第 slot 槽（`memmove(rec+2s+10, rec+2s+12, 2*(7-s))`），末字节 `rec[24]=0x80`；返回 dest |

合计 66 个调用点、264 字节 obj0 机器码；记录基址一律 `dword_53A45 + 80u*(uint32_t)index`
（机器码 `i*5 << 4`，按 32 位无符号算）。依赖只有 Watcom 栈探针 `0x3702F`（约定不转译）
和 CRT `memmove`（`0x3771C`，C 侧用宿主 `memmove`，重叠语义一致）。

IDA 逐条核对确认了三个易错点，全部按机器码原样实现：

- **`0x344F2` 的 `value` 高半字节绕过掩码**：机器码是 `and bl,0F0h; or bl,cl`，
  **没有**先把 value 截到 4 位 —— `0x55`/`0xF0`/`0xFF` 的高位会 OR 进去。C 只写
  `| (uint8_t)value`。
- **`0x344F2` 的区间比较是 `jle`（有符号）**：`start > end` 跑零次；C 用 `int i` 复现。
- **`0x1B8E7` 的长度按 32 位无符号算**：`mov eax,7; sub eax,slot; add eax,eax`。
  slot 取 0..7 时长度 14..0；越界时原版照样下溢，C 用 `2u*(7u-(uint32_t)slot)` 逐位复现。

## 58.2 对拍（`reccheck` 扩展）

沿用既有骨架（`le_reserve_address_space` → `le_open` → `le_map_and_relocate`；
synthetic 表 + 真游戏全局 `dword_53A45`）。写函数在**两份逐字节相同的副本**上各跑一遍
（原机器码跑 `obuf`、C 跑 `cbuf`），判据 = **返回值 + 整块 64 记录 × 80 字节缓冲区逐字节**
（`rec_slot_remove` 的返回指针先归一化成缓冲内偏移再比）。

| 函数 | 用例 |
|---|---|
| `rec_field_byte` | 随机 `index∈0..63`、`slot∈0..7`（800 例），槽字节全值域 |
| `rec_status_set` | 随机 `start/end∈0..63`（含 `start>end`）、`value∈{00,0F,55,A0,F0,FF}`（800 例）+ 构造 `start>end` / `start==end` / 全范围 3 例 |
| `rec_slot_claim` | 随机 800 例 + 4 记录 ×（全满 + 单空槽 8 路） |
| `rec_slot_remove` | 随机 800 例 + 4 记录 × 全 8 槽（含 slot 7 的零长拷贝） |

结果 **32010 cases, 0 failures**（28739 → +3271）。同轮 `mapcheck` **97000/0** 与
`utilcheck` **2200/0** 仍过（`mapcheck` 链接 `rec.c`，确认新增函数未扰动旧符号）。

## 58.3 一起踩的坑

- **构建 `fd2host` 前先确认没有残留的 `fd2host.exe` 在跑**：上一会话留下的进程会锁住
  `build/fd2host.exe`，链接直接 `LNK1104: 无法打开文件 fd2host.exe`，
  看着像构建脚本坏了，其实是文件被占用。`taskkill /IM fd2host.exe /F` 后重建即好
  （`PITFALLS` §8-72）。

## 58.4 判据

| 项 | 结果 |
|---|---|
| `reccheck` | **32010/0**（28739→+3271） |
| `mapcheck` / `utilcheck` | 97000/0、2200/0 |
| `regress.ps1` | **8/8 PASS**、`repl: installed 85`、`FD2.TMP=207360` |
| A/B 同 tick `--shot-tick=500` | `none↔none2` 0 px、`none↔all` **0 / 64000 px** |
| Linux | `make -f Makefile.linux` 构建通过、`letest-linux` 三对象 exact match、`doscheck-linux` **49/49** |
| `translation_map` / `func_ranking` | **85 wired / 1359（6.3%）** |

进度 **85 / 1359（6.3%）**。

## 58.5 下一步

同族、同依赖闭合、同样进 `rec.c` + `reccheck` 的还有：
`0x1B8A6`(65B, 统计占用槽数)、`0x1B83D`(105B, 找 `(p[0]&0x40)` 且 `p[1]` 相对 `0x80`
满足谓词的槽)、`0x1CA89`(依赖已接入的 `0x4E866`)、`0x13512`/`0x32975`/`0x34D64`/`0x35009`
（记录 +5/+52 单字节置位叶子）。

再往上是三个**故意后置**的方向：`0x1956B`/`0x1974C`/`0x26996`（消息框开/关，usage 103，
`malloc(64000)×3` 需走 `guest_mem`）、`0x112A5`+`0x1145A`（单位记录初始化，609 B）、
`0x11EEE`+`0x24D22`（地图格绘制中间件，1093 B）。
