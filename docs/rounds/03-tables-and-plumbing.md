# 轮次明细：地基与管线
§24 地形代价洪泛/寻路、§25 资源加载器 + **CRT 重定向对拍术**、§26 转译函数接入宿主（`src/repl.c`）、
§27 表访问器收尾。
---

## 24. 第 24 轮：地形代价洪泛 / 寻路簇转译（0x4E390..0x4E751）（2026-10-05）

第 22 轮地形图里价值最高的一块：`0x4E390..0x4E751` 是**两个入口 + 七个内部函数**，
实现"单位用剩余移动点在带地形代价的格子上做 DFS 洪泛（可达范围）+ 第二遍沿路径回溯并
提交最优路线"——即**移动范围与寻路**。两条入口都是 cdecl（`pusha`/`popa` 全保存），
内部函数走 usercall 寄存器、用 `unk_60079` 当显式回溯栈。

### 24.1 数据结构（从指令逐条推出）

- **地图** `map`：`[+0]=W(u8) [+2]=H(u8)`，`[+4]` 起是 `W*H` 个 **4 字节格子**；
  原版格子指针用 `map + 7 + 4*(y*W+x)`（指向格子**最后一个字节**），所以：
  `cell[-3]`=byte0（地形 id 低字节）、`cell[-2]`=byte1（bit0-1 地形高位→随后成了转向计数 bits2-7）、
  `cell[-1]`=byte2（标志：`0x40`=目标、`0x80`=剩余清零）、`cell[0]`=byte3（剩余移动点）。
- **地形代价**：`t = ((cell[-2]&3)<<8) | cell[-3]`；`cost = cost_row[ cost_table[4*t+1] ]`。
  `cost_table`（原 dword_60060）是地形 id → 代价行下标的映射，`cost_row`（入口 arg0/esi）
  是本单位每类地形的代价。
- **邻居顺序**：右(+4)、左(-4)、下(+4W)、上(-4W)；方向码 右=3/左=1/下=0/上=2。
- 第二遍记录 8 字节 `[dx:2][cl:1][ch:1][cell:4]`；`byte_60077`=深度、`byte_60078`=当前最优路径长、
  `dword_60073`=路径输出缓冲。`0x4E71F` 数当前栈里的**转向次数**（×4），`0x4E751` 在到达目标且
  深度更短时把方向码拷进输出，`0x4E703` 是 mode2 的目标标记。
- 只有"剩余点严格变优"（第二遍允许等点但转向更少，mode1）才继续深入 ⇒ 深度受剩余移动点约束。

### 24.2 转译：`src/game/path.c`

| 转译名 | 原地址 | 角色 |
|---|---|---|
| `path_mark` | `0x4E390` | 入口：一遍洪泛标记可达范围 |
| `path_find` | `0x4E4F6` | 入口：寻路 + 提交最优路线（返回最优长度） |
| `rec1` / `check1` | `0x4E42C` / `0x4E4BE` | 第一遍递归 / 可达性与剩余点更新 |
| `rec2` / `check2` | `0x4E5CC` / `0x4E680` | 第二遍递归 / 带转向择优的检查 |
| `mark_target` / `count_dirs` / `commit_path` | `0x4E703` / `0x4E71F` / `0x4E751` | 目标标记 / 转向计数 / 路径提交 |

原版的 usercall 寄存器参数与显式回溯栈在 C 版换成普通参数与类型化栈（只有 map、输出缓冲、
`byte_60078` 对外可见，观测等价）。**逐条指令对齐**，未引入任何"猜"的高层语义。

### 24.3 对拍（`src/pathcheck.c` + `build.ps1 -Target pathcheck`）

随机小地图（W/H 4..7、格子字节随机、地形代价 1..5、剩余点 1..12 保证深度小）、随机起点/目标/mode。
两条入口都跑原机器码与 C 版，比较 **map 全量 + 输出缓冲 + 返回的最优长度 + 深度全局**。

```
build\pathcheck.exe  →  PASS: 1000 cases, 0 failures（首轮即通过）
```

### 24.4 实测判据

```
pathcheck 1000/0   utilcheck 2200/0   sprite24check 2100/0   gfxcheck 1450/0   rlecheck 1900/0
regress.ps1        ALL PASS 8/8
```

### 24.5 下轮入口

1. 表访问器 `0x4E7DD..0x4E8BC`（`&unk_XXXX + 步长*i`，无逻辑）；计时器/调色板动画
   `0x4E310/0x4E31C`、清键盘 `0x4E381`（需平台层）。
2. ★★★ `sub_111BA`（资源加载，内部走 Watcom CRT 文件服务，独立对拍需先接宿主 `dos.c`）、
   `sub_15F84`（文本/脚本渲染，usercall 多寄存器）。
3. 宿主侧：可开始把已转译的纯模块（rle/gfx/sprite24/util）逐步**接入宿主**（替换原机器码），
   每接一个跑 `regress.ps1` + 帧对拍。

---

## 25. 第 25 轮：资源加载器 `sub_111BA` 转译 + CRT 重定向对拍术（2026-10-05）

### 25.1 `sub_111BA` 的真实 ABI（从调用点实证）

Hex-Rays 把它读成 7 参 `__fastcall` 是错的（入口的 `push 20h; call sub_3702F` 是 Watcom
**栈探针**，把签名骗了）。看调用点（如 `0x10136`）只有 3 次 `push` ⇒ **cdecl 3 参**：

```
void *res_load(const char *filename, void *old_buffer, int index)
```

语义（逐指令）：`free(old_buffer)` → `fopen(filename,"rb")` → `malloc(8)` →
`fseek(4*index+6)` → 读 8 字节 `{start,end}` → `size=end-start`（写全局 `dword_53BFF`）→
`malloc(size)` → `fseek(start)` → 读 size 字节 → `fclose` → 返回缓冲。失败走 `0x1005E`
（`push 1; jmp exit`）。**LMI 容器**：`+6` 起是 `count+1` 个 u32 偏移，资源 `i` 占
`[off[i], off[i+1])`。`sub_3702F/sub_37042` 只是栈探针（实测无害）。

### 25.2 新方法：**CRT 重定向对拍术**（本轮最大收获）

`sub_111BA` 依赖 Watcom CRT 的文件/内存函数（`fopen/fseek/fread/malloc/free/fclose`），
直接跑会陷进 INT 21h（需要 DOS 层），且 Watcom `malloc` 没经过 CRT 启动可能不可用。
本轮改用**只重定向 CRT、不碰游戏代码**的办法：

1. LE 加载并用 `le_map_and_relocate` 映射 FD2.EXE；
2. 把上面 6 个 CRT 入口（`0x3706E/0x3776E/0x37324/0x3759C/0x37940/0x373CA`）头 5 字节
   改写成 `E9 rel32` jmp 到宿主 libc 的薄封装；
3. 之后直接调用**原版机器码**，它的文件/内存调用走到 MSVC 的 `fopen/fread/...`。

这恰好就是"最终移植要做的替换"（用现代 libc 换 Watcom CRT），而且**不改任何游戏逻辑**，
只在函数入口写等长 jmp（不动 call 位移），符合 §4 的硬约束意图。Watosn `malloc` 初始化问题
被绕过；**任何依赖 CRT 的游戏函数现在都能这样对拍**（`sub_15F84` 等下一步可用）。

### 25.3 转译与对拍

| 文件 | 内容 |
|---|---|
| `src/game/res.h` / `res.c` | `res_load`（原 `0x111BA`）+ `res_size`（原 `dword_53BFF`） |
| `src/rescheck.c` | 造合成 LMI 容器（8 资源 × 随机长度）：原版经 CRT 钩子 vs 转译 C，比对资源内容与大小 |
| `re/util_disasm.txt` | 已含本轮前归档；`sub_111BA` 反汇编见会话记录/`re/`（可补归档） |

### 25.4 实测判据

```
build\rescheck.exe  →  PASS: 160 cases, 0 failures（首轮即通过）
pathcheck 1000/0  utilcheck 2200/0  sprite24check 2100/0  gfxcheck 1450/0  rlecheck 1900/0
regress.ps1         →  ALL PASS 8/8
```

### 25.5 下轮入口

1. 用同一 CRT 重定向术把 **`sub_111BA` 接进宿主**（宿主启动时替换这 6 个 CRT 入口，
   游戏即用转译的资源加载）——这是"真正在跑转译代码"的第一步，且几乎零风险（纯等价替换）。
2. ★★★ `sub_15F84`（文本/脚本渲染，1380 B usercall）：可先用 CRT/内存重定向 + 全局状态快照
   做对拍（比 path 簇更复杂，需先归档它写到的全局区 `0x53xxx`）。
3. 表访问器 `0x4E7DD..0x4E8BC`；计时器/调色板动画 `0x4E310/0x4E31C`（需端口/时间，可用同样的
   "重定向依赖"术测其表逻辑）。

---

## 26. 第 26 轮：转译函数接入宿主 + `--exit-after` 早退 bug 修复（2026-10-05）

### 26.1 先修 bug：无 `--exit-when-file` 时 `exit_file_ok()` 返回 1 → 任何 `--exit-after` 运行 2 s 就退

`exit_file_ok()` 在未给文件条件时 `return 1`，于是看门狗的完成触发器
`g_autokey_done && exit_file_ok()` 立刻为真 → 所有只带 `--exit-after` 的运行在
`EXIT_SETTLE_MS=2000` 后提前退出（约 74 帧），与 README「`--exit-after=30`」矛盾。
**修复**：无路径时 `return 0`（没有文件条件就永远不算"写满"）。验证：

```
--exit-after=5 运行 → elapsed=5.1 s，watchdog fired after 5 s (159 frames drawn)
```

`regress.ps1` 传了 `--exit-when-file`，不受影响。同轮给 `regress.ps1` 加了
`-Replace ""|none|all|groups` 参数，便于 A/B。

### 26.2 接入：`src/repl.c` —— 让游戏真的跑转译代码

新增替换层：把每个已通过机器码对拍的转译函数的**入口**改写成 5 字节 `jmp rel32`
指向 C 实现（与 `src/ail.c` 替换 AIL 入口同一机制），在 `le_map_and_relocate` 之后、
游戏线程启动之前安装。共 **23 个**：

- `rle`：`0x4E98D`/`0x4E8D3`；`gfx`：`0x4EC7C/0x4ECBF/0x4ED0B/0x4ED34/0x4ED7A/0x4EEE0`；
- `sprite24`：`0x4DF84/0x4E016/0x4E0A2/0x4E127/0x4E1A6/0x4E22A/0x4E29C`；
- `util`：`0x4DED4/0x4DEEC/0x4DF09/0x4DF28/0x4DF4C/0x4E795`；`path`：`0x4E390/0x4E4F6`。

**安全性论证**（写进 `src/repl.h`）：这些是普通 cdecl 函数，MSVC 实现保留的 callee-saved
寄存器是原版保证的超集（唯一例外 `0x4DF09` 原版不保存 EBX，我们的 C 保存，安全）；
**ida 实证：没有任何被替换集合之外的代码读取这些 scratch 全局**（`0x627A3..0x627B6`、
`0x6017B`、`0x60060..0x6017A` 的 xref 全在集合内）；只对 `FD2.EXE` build 生效（地址是 FD2 的）。
`--replace=none|all|groups` 可切换（默认 all）。**`src/game/res.c` 暂不接入**：原版走 Watcom
堆分配、游戏再用同一堆 free，换成 libc malloc 会混堆，等 CRT 堆一起替换时再接。

### 26.3 A/B 验证

```
pwsh -File regress.ps1 -Replace none  → ALL PASS 8/8（repl: disabled）
pwsh -File regress.ps1 -Replace all   → ALL PASS 8/8（repl: installed 23）
```

- regress 尾帧不可直接比：它是"退出前最后一帧"，动画相位不同 ⇒ 两次 none 也差 **17.9%** 像素。
- 改为**固定帧 150**（`--shot-frame=150` + 指向不存在文件的 `--exit-when-file` 抑制早退）：

| 对比 | 差异像素 |
|---|---|
| none vs none | 143 / 64000（0.2234%） |
| all vs all | 224 / 64000（0.3500%） |
| **none vs all** | **143 / 64000（0.2234%）** |

⇒ none↔all 的差异**不超过 none↔none 的基线**（游戏自带 ~0.2% 帧定时噪声），转译未引入额外差异；
结合 23 个函数已逐字节对拍，判定**接入等价**。

### 26.4 实测判据

```
fd2host: repl: installed 23 translated function(s) (mask 0x1F)
regress.ps1 (-Replace none / all): ALL PASS 8/8
rlecheck 1900/0  gfxcheck 1450/0  sprite24check 2100/0  utilcheck 2200/0  pathcheck 1000/0  rescheck 160/0
```

### 26.5 下轮入口

1. 继续转译 ★★★ `sub_15F84`（文本/脚本渲染）——现在可用「CRT 重定向 + 固定帧对拍 + 接入 repl」
   的完整链路；表访问器 `0x4E7DD..0x4E8BC` 顺带清掉。
2. 把 CRT 堆（`malloc/free/_nmalloc/...`）与文件层整体替换为宿主实现，届时 `res.c` 也可接入。

---

## 27. 第 27 轮：表访问器收尾 + 接入 34 个转译函数（2026-10-05）

### 27.1 转译：`src/game/tables.c`

obj0 库区最后一块：`0x4E7DD..0x4E8BC` 的 **11 个一行访问器**，都是 `base + stride*index + offset`
（32 位无符号乘，原版 `mul edx`），其中 `0x4E87D` 是 4 字节表的 dword 读取。基址/步长/偏移见
`game/tables.h`；C 版把基址参数化（数据表将来变成 C 数组），`repl.c` 提供 FD2 固定基址。

| 地址 | base | stride | offset |
|---|---|---|---|
| `0x4E7DD` | 0x615FE | 2 | −64 |
| `0x4E7F2` | 0x626B3 | 12 | 0 |
| `0x4E809` | 0x6238D | 31 | −31 |
| `0x4E821` | 0x620A1 | 11 | 0 |
| `0x4E838` | 0x61DA1 | 24 | 0 |
| `0x4E84F` | 0x61AF9 | 10 | 0 |
| `0x4E866` | 0x619FD | 7 | 0 |
| `0x4E87D` | 0x61955 | （dword 表，×4） | 0 |
| `0x4E88E` | 0x6188A | 7 | 0 |
| `0x4E8A5` | 0x61646 | 20 | 0 |
| `0x4E8BC` | 0x602AD | 23 | 0 |

至此 **obj0 工具库 0x4DED4..0x4EEE0 全部转译完成**。

### 27.2 对拍与接入

`src/tablescheck.c`：每个访问器取边界值（0/1/0x1F/0x20/0xFF/负值）+ 400 随机索引，
与 C 辅助函数比对返回值（指针或 dword）：

```
build\tablescheck.exe  →  PASS: 4528 cases, 0 failures（含 11 个访问器）
```

11 个访问器加入 `src/repl.c`（`REPL_UTIL` 组）⇒ 接入数 **23 → 34**：

```
host.log: repl: installed 34 translated function(s) (mask 0x1F)
regress.ps1: ALL PASS 8/8
```

### 27.3 实测判据

```
tablescheck 4528/0   rlecheck 1900/0   gfxcheck 1450/0   sprite24check 2100/0
utilcheck 2200/0     pathcheck 1000/0  rescheck 160/0
regress.ps1 ALL PASS 8/8（34 个转译函数在跑）
```

### 27.4 下轮入口

1. ★★★ **`sub_15F84`（文本/脚本渲染器）**：已归档反编译 `re/sub_15F84.c`。它是 14 参数的
   usercall **词流解释器**（switch on `*v15`：`-1` 结束/`-2` 换行/`-3` 换行+等键翻页/
   `-4..-6` 数字/`-17..-20` 开对话框（4 种肖像来源）/默认字形索引），递归、写大量 `0x53xxx`
   全局、调用 `sub_165AC/168B6/16B43/16C57/16559/16E24/164E8/12C60/111BA/4EBFF/4EC31/4ED7A/10620`。
   属多轮工程：先归档全局区与各 callee 契约，再分子块转译+对拍。
2. 表访问器已完成；CRT 堆/文件层整体替换仍是让 `res.c` 接入的前提。
