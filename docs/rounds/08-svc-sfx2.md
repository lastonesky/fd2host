# 轮次明细：`0x25B45`（`svc_play_sfx` 的同形函数）
§37 把 `0x25A96 svc_play_sfx` 的孪生函数 `0x25B45` 一并转译接入；
顺带纠正 `sub_15F84` 脚本 VM 的 ABI 测绘（`RE_MAP` 原记"14 个寄存器参数"是错的）。
---

## 37. 第 37 轮：`svc_play_sfx2`（`0x25B45`）转译 + 对拍 + 接入（2026-10-06）

### 37.1 背景

`docs/TRANSLATION.md` §5 第 4 条："`0x25B45`（`0x25A96` 的同形函数）与 `0x25A96` 合并处理"。
`0x25A96`（`svc_play_sfx`）第 33 轮已转译并接入，但孪生函数一直留在原机器码 —— 游戏里
**两条 SFX 通道**各有一套播放例程。

### 37.2 测绘：真的是"同形"吗（逐字节）

IDA 里两个函数都是 175 字节，逐字节 diff 得 **17 个不同字节**，按指令归类：

| 差异来源 | 字节数 | 说明 |
|---|---|---|
| `call rel32` 操作数 | 12 | 6 条调用（栈探针 `0x3702F` + 5 个 AIL 入口）的 rel32 低 2 字节 —— **同一目标、不同调用点** |
| `push [imm32]` 操作数 | 5 | 5 处 `push dword_53EE4` → `push dword_53EE8`（编码 `FF 35 …`，只有最低字节 `E4`→`E8`） |
| 其余 | 0 | **结构完全相同** |

参数也相同：调用点都是 `push loops; push index; push bank; call …; add esp,0Ch`
（`bank` 一律是 `dword_53EEC`，即 FDOTHER.DAT 的样本容器）。两个句柄在 `0x25C43`/`0x25C57`
背靠背分配（`mov dword_53EE4/eax`、`mov dword_53EE8/eax`）。调用点：`0x25A96` **111** 个、
`0x25B45` **11** 个。

### 37.3 顺带纠正：`sub_15F84` 的 ABI 不是"14 个寄存器参数"

`re/RE_MAP.md` 原来记 `0x15F84` 是 `__usercall` **14 个寄存器参数**。本轮取样点核对时发现：

- 调用点只有 **9 个 `push`**，紧跟 `call`，之后 `add esp,24h`（9×4=36）—— 典型 **cdecl**；
- 函数入口是 `push 5Ch; call sub_3702F`，而 `sub_3702F` → `sub_37042` 是 **Watcom 栈探针
  `_chkstk`**（检查 `&retaddr - size` 与栈极限 `dword_52814`、必要时逐页碰栈）；
- IDA 把探针的原型（EAX 进出）传播成了被调函数的"寄存器参数"，于是每个以
  `push <帧大小>; call 0x3702F` 开头的函数都被标成 `__usercall`。
  `svc.c` 第 33 轮其实已经写对过这件事（"stack probe, no-op for the caller"），这次把它固化成测绘结论。

⇒ `sub_15F84` 的真实签名是 **9 个栈参数**（`stream, index, x0, …`，首参即词流基址：
`mov esi,[esp+34h+arg_0]; movsx eax,word ptr [esi+eax*2]; add esi,eax`）。已更正
`re/RE_MAP.md` 与 `docs/TRANSLATION.md` §5 第 1 条 —— **下一轮（脚本 VM）按 cdecl 做**。

### 37.4 转译写法（`src/game/svc.c`）

不复制两份，抽一个共享体、**把"句柄槽"当参数**：

```c
static int play_sfx(void **slot, const void *bank, int index, int loops);
int svc_play_sfx (const void *b, int i, int l) { return play_sfx(&dword_53EE4, b, i, l); }
int svc_play_sfx2(const void *b, int i, int l) { return play_sfx(&dword_53EE8, b, i, l); }
```

传 `slot` 而不是先取值，是因为**原机器码每条 AIL 调用都重新读一次句柄全局**
（`push dword_53EE8` 出现 5 次），宏 `dword_53EE8` 每次解引用才是逐字等价。

### 37.5 对拍（`typecheck` 扩到 4 条路径）

`test_sfx` 改成带 `path/orig/cfun` 参数的共用测试，两条通道各跑一套：

- **句柄必须不同**：`G53EE4 = 0x40001000`、`G53EE8 = 0x40008000`（在用例开始前显式设置），
  AIL 桩把句柄记进事件日志 —— 两个函数**用错句柄会被立刻抓到**；
- 随机 400 例 + 构造矩阵（3 个门开关 × 5 个 index，含 `-1` 与边界 7）各跑一遍；
- 其余照旧：整帧 320×200 VGA、返回值、`dword_53A10/53A14/53A2C`、最终 tick、**完整事件序列**。

```
paths: wait=90 sfx=440 sfx2=440 step=646
PASS: 1616 cases, 0 failures        （改前 1176，新增 440 例）
```

### 37.6 接入与回归

| 项 | 结果 |
|---|---|
| `repl.c` | 新增 `{ 0x25B45, "svc_play_sfx2", …, REPL_SVC }`（入口首指令 `push 1Ch` 正好 5 字节，无内部跳入点） |
| 日志 | `repl: installed 50 translated function(s) (mask 0xFF)`（49 → **50**） |
| A/B 抓帧 | `--replace=none` vs `all`，`--shot-tick=600` 静止画面 → **0 / 64000 px（0.0000%）** |
| 回归 | `regress.ps1` **ALL PASS（8/8）**、`FD2.TMP = 207360` |

### 37.7 下轮入口

`svc` 组 3 个函数全部转译完毕，`sub_15F84` 脚本 VM 的依赖只剩快照/还原
`0x15E9E`/`0x15E71`（按计划与 CRT 堆整体替换一起接）。ABI 结论见 §37.3，
方法与判据见 `docs/TRANSLATION.md` §5 第 1 条。
