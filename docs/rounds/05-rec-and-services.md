# 轮次明细：角色记录与系统服务
§32 角色记录表（`rec_flag`/`rec_find`）、§33 打字机步进 + tick 等待 + PCM 音效（确定性时钟对拍）。
---

## 32. 第 32 轮：角色记录表转译（`0x34894 rec_flag` / `0x12C60 rec_find`）（2026-10-05）

### 32.1 转译：`src/game/rec.c`

两张 **80 字节记录表**住在数据段，由 `sub_10010` 从存档块填充（逐条从反编译钉死）：

| 全局 | 含义 | 来源（`sub_10010`） |
|---|---|---|
| `dword_53A45` | 表 1 基址 | `memmove(dword_53A45, blob + 4771, 80 * dword_53BEB)` |
| `dword_53BEB` | 表 1 记录数 | `blob[12484]`（无符号字节） |
| `dword_53BF7` | 表 2 基址 | `memmove(dword_53BF7, blob + 2211, 2560)`（= 32 条） |
| `dword_53BFB` | 表 2 记录数 | `blob[12492]`（无符号字节） |
| `dword_53C1B` | 最近匹配的记录 | `rec_find` 写入；调用方之后读它取头像 |

观察到的记录字段：`+0/+1` 一个 word（`sub_12C0D` 拿它和 `qword_53AB1` 比）、`+2` 图标索引（来自 `FD.ICON.B24`）、
`+5` 标志字节、`+7` 对话渲染用的 DATO 资源索引、`+8` 这两个函数检索的 id。

两个语义细节（反汇编钉死，见 `re/rec_functions.txt`）：

- `rec_flag(i)`：`mov eax,edx; shl eax,2; add edx,eax; shl edx,4` —— 即 `i*5 << 4` = **无符号** `80*i`，取 `byte+5 & 1`。
  C 侧按 `(uint32_t)index * 80` 逐字复现：负索引会回绕成表外地址（两侧都会 fault），能断言的只有回绕本身。
- `rec_find(want)`：先 `dword_53C1B = 0`；扫表 1，凡 `byte+8 == want`（**无符号字节**比较，所以 `want > 255` 永不命中）
  就把记录地址记进 `dword_53C1B`，并在该记录 flag bit0 == 0 时立刻返回索引；**表 1 一个都没命中**才扫表 2，
  而第二段循环**不提前退出** —— `dword_53C1B` 最终停在**最后一条**匹配记录，返回值仍是 `-1`。
  调用方（`sub_15F84`）只把返回值当"找到未标志记录"用，之后从 `dword_53C1B` 读头像。

写法沿用 §30.1 的 app-level：C 读写与原机器码同一批数据段全局，`repl.c` 无需胶水。

### 32.2 对拍（`src/reccheck.c` + `build.ps1 -Target reccheck`）

表内容是合成 buffer，但**全局是真正的游戏全局**：两侧读同一个数据段，所以这是真对拍而不是算法复述。
比较返回值 + `rec_find` 留下的 `dword_53C1B`。覆盖：随机表/记录数/want，加上每条路径的构造用例
（全不命中、只在表 2 命中且有多条、表 1 先命中已标志再命中未标志、表 1 全标志（于是表 2 不被扫）、
`want > 255`、空表）。按 §22.4，该 console 目标必须 `/link /BASE:0x60000000`，否则 exe 自己会落进 guest 窗口。

### 32.3 实测判据

```
le: fixups applied=7937, cross-page records skipped=22, pages with leftover data=0, bad records=0
paths: random=900 none=1 table2=1 flag0=1 flag1_only=1 want>255=1 empty=1
PASS: 28739 cases, 0 failures
repl: installed 46 translated function(s) (mask 0x7F)   # 新增 REPL_REC = 0x40
regress.ps1 ALL PASS 8/8（15.2 s），FD2.TMP = 207360 bytes（与原件同尺寸）
```

### 32.4 下轮入口

1. `sub_164E8`（113 B，单调用点 = `sub_15F84`）—— 逐字符打字机步进：每 2 个字符贴一次嘴型 DATO 子图、
   播音效表第 2 项、延 1 tick。依赖 `sub_25A96`（AIL 音效服务）与 `sub_17AA9`（tick 延时），见 §33。
2. 之后整体转译 `sub_15F84`（1380 B 词流解释器，126 个调用点）并对拍 VGA。
3. 排期项（不阻塞）：CRT 堆 + 文件层整体替换后，`res.c`、`0x15E71/0x15E9E` 一起接入。

---

## 33. 第 33 轮：打字机步进与两个系统服务（`0x164E8` / `0x25A96` / `0x17AA9`）（2026-10-06）

### 33.1 转译：`src/game/svc.c`（新模块）+ `dlg_type_step`（`src/game/dlg.c`）

按 §32.4 的下轮入口，先把 `sub_164E8` 依赖的两条服务各自钉死，再转译步进本身。三条函数的
机器码见 `re/round33_disasm.txt`，全局的写入点见 `re/round33_writers.txt`。

**`0x17AA9` → `svc_wait_ticks(int n)`**（68 B，40 个调用点）。开头的
`push 4; call sub_3702F` 是栈探针（`xchg eax,[esp+arg_0]` → 探针体 → 取回原 eax → `retn 4`，
对调用者是纯 no-op），C 侧没有对应物。语义：

```c
dword_53A2C = (int16)tick;                 /* movsx，16 位有符号 */
do {  elapsed = (int16)tick - dword_53A2C;
      if (elapsed < 0) elapsed += 0x10000; /* 0x7FFF -> 0x8000 的回绕 */
} while (elapsed < n);
dword_53A2C = (int16)tick;                 /* 重读一次，同时也是返回值 */
```

`+0x10000` 只在**有符号**回绕处生效：读数在 0x8000 以下为正、以上为负，只有跨过
0x7FFF/0x8000 时差值才是负的；跨 0xFFFF/0x0000 时两头都是负数、差值天然为正（§33.3 的教训）。
`n <= 0` 直接返回；返回值（最后一次读数）按 IDA 扫过全部调用点**无人使用**。

**`0x25A96` → `svc_play_sfx(bank, index, loops)`**（116 B，15 个调用点）：

| 门 / 数据 | 地址 | 来源（`main`） |
|---|---|---|
| 数字驱动已起来 | `byte_53EF1` | `0x25C3B mov byte_53EF1,1`（拿到驱动句柄之后） |
| 音效开关 | `byte_51E62` | 选项面板第 1 项，`docs/data/ida/fd2_system_overlay_options_ida.txt` 已证 |
| 忙 / 闸门 | `dword_54133` | 多处写（`0x2E735` / `0x31AE4`），非 0 就不起播 |
| 样本句柄 | `dword_53EE4` | `0x25C43 AIL_allocate_sample_handle` |
| SFX 容器 | `dword_53EEC` | `FDOTHER.DAT` #0x1F（`0x25C65..0x25C78`） |

容器布局由读取方式钉死：`row = bank + 4*index`，取 `row+6`（起始）与 `row+0xA`（结束）两个
dword —— 即"6 字节头 + 比样本数多一个的偏移表"，样本 i 是 `[off[i], off[i+1])`。调用顺序
`stop → (index != -1) → init → set_address(h, start, end-beg) → set_loop_count(h, loops) → start`；
`index == -1` 只停不播。同形的 `0x25B45`（句柄 `0x53EE8`、闸门 `0x540FF`）本轮未动。

**`0x164E8` → `dlg_type_step(void)`**（113 B，唯一调用点 `sub_15F84`），每调用一次贴出一个字符：

```c
if (++dword_53A14 == 2) {                            /* 每两个字符动一次嘴 */
    if (++dword_53A10 == 4) dword_53A10 = 0;
    idx = (dword_53A10 == 3) ? 1 : dword_53A10;      /* 贴出的子图序列 1,2,1,0 循环 */
    dlg_blit_dato(dword_53A85, dword_53C67, idx);
    dword_53A14 = 0;
}
svc_play_sfx(dword_53EEC, 2, 1);                     /* 打字音：容器第 2 项，循环 1 */
return svc_wait_ticks(1);
```

`dword_53A10` / `dword_53A14` 的 xref 全在 `0x164E8` 内部（私有计数器），但仍是真实全局 ——
这个循环的意义就在于跨调用留状态。**越界也照做**：初值落在 0..3 之外时（例如 4）`++` 后不等于
4，于是 `idx` 直接取到 5、6、7…，两侧都会照这个索引去读 DATO 表（§33.2 的用例为此留了余量）。

### 33.2 对拍（`src/typecheck.c` + `build.ps1 -Target typecheck`）：确定性时钟

`svc_wait_ticks` 是忙等 BIOS tick 的，裸跑两次既不确定、也不可能（BDA `0x46C` 在低 64 KiB）。
沿用 §31.2 的低内存镜像 + 操作数重定向（FD2 build 实测 **65 处**），再加上一样 keycheck 不需要
的东西 —— **一个时钟**（继 §25.2 CRT 重定向、§31.2 低内存镜像之后的第三种对拍手法）：

1. 原机器码 `0x17AA9` 里的三处 tick 读取（`0x17AB7` / `0x17AC4` / `0x17ADF`）整段换成
   `call tick_read; nop*3`，C 侧读 tick 用的 `0x4E310` 也 hook 到同一个桩：**一个桩、一个时钟**。
2. `tick_read` 每被调用一次就把 tick 加一：于是 tick 序列只取决于"读了几次"，多读或少读一次
   立刻体现为不同的 tick、不同的 `dword_53A2C` 和不同的循环次数 —— 与 MSVC 编出来的指令数
   无关，这才是 C 能和原机器码逐字节相比的前提。
3. `tick_read` 是 **naked 包装**（`push ecx; push edx; call 实现; pop edx; pop ecx; ret`）：
   必须保住 EDX —— 原机器码的等待循环把 `n` 放在 EDX 里，桩改掉它的话跑飞的是原程序而不是 bug。
4. 五个 AIL 入口（`0x39521` / `0x39694` / `0x39AAE` / `0x39798` / `0x39805`）换成记录桩
   （记函数名 + 句柄 + 相对容器的偏移 + 长度 + 循环数），对拍环境里没有声卡驱动。

比较项：整帧 VGA、`dword_53A10` / `53A14` / `53A2C`、最终 tick、返回值，以及**完整的事件日志**
（每次 tick 读取 + 每个 AIL 调用及其实参，按序）。

```
le: fixups applied=7937, cross-page records skipped=22, pages with leftover data=0, bad records=0
typecheck: redirected 65 low-memory references to 0x70000
paths: wait=90 sfx=440 step=646
PASS: 1176 cases, 0 failures
```

**坑 1（本轮最大的一个）**：那三处 tick 读取是 **8 字节**（`B8 imm32` 5 字节 + `0F BF 00`
3 字节），第一版按 6 字节打补丁，剩下的 `BF 00` 被当成指令执行，跑到 `0x17AC7` 时 AV
（读 `0x3A2CA300`）。`0x17AC4` 同时是跳转目标（循环头），所以只能整段替换、不能只改起始字节。

**坑 2**：随机 DATO 表只有 4 项时，`dword_53A10` 初值为 4 的用例会在第 8 步读到表外（索引 8）
→ 读乱指针 AV。改成 16 项（8 步 × 每两步加一，最大索引 8）之后两侧都落在合法内存里。

### 33.3 变异验证：这把尺子确实量得出东西（三条全被抓到）

| 变异 | 结果 |
|---|---|
| `+0x10000` 改成 `+0xFFFF` | **FAILED**：40 cases / 8 failures（tick 序列错位） |
| 去掉 `idx = (phase == 3) ? 1 : phase` | **FAILED**：`VGA diff @+36874 orig=AF ours=14` |
| `end - beg` 改成 `beg - end` | **FAILED**：`event 2 orig=address(...,362,48) ours=address(...,362,-48)` |

第一条有个真教训：补覆盖之前它**是 PASS 的** —— 原来的等待用例只在 0x0000 / 0xFFF0 附近起
tick，永远绕不过 0x7FFF/0x8000，那段修正代码根本没被执行过。加了 `0x7FF8 + rand` 这个窄窗口
（每个 run 大约读十次）才真正覆盖到，变异也随之暴露。

### 33.4 宿主实测：接入数 46 → 49（mask `0x7F` → `0xFF`）

`repl.c` 新增 3 条（新分组 `REPL_SVC = 0x80`；`dlg_type_step` 归 `REPL_DLG`）：

```
repl: installed 49 translated function(s) (mask 0xFF)
host: exit condition met after 14 s (465 frames drawn)
FD2.TMP = 207360 bytes（与原件同尺寸）
```

8 项断言全 PASS（AH=3C 已发出、创建后重开成功、无 UNHANDLED INT21、无 cpu 崩溃、无未处理
异常、FD2.TMP 存在且非空、正常退出）。

宿主 A/B（标准回归配方 `--autokey=5000:SPACE;2500:RETURN;2500:RETURN;2500:DOWN,RETURN;3000:RETURN×5`，
`--shot-frame=1500`）：

| 对比 | 结果 |
|---|---|
| 帧 1500：`all` vs `none` | **0 / 64000 px（0.0000%）**，`framediff.ps1` |
| 帧 1450：`all` vs `none` | **0 / 64000 px** |
| 日志计数 `all` vs `none` | 播放 51 / 51，stop 53 / 53，帧数 1919 / 1921，无 UNHANDLED、无 `cpu:` |

**一个要记住的负结果**：把配方加长（末尾再补三次 `;2000:RETURN`）之后，`all` 与 `none` 的
`ail: play` 计数差了 8 倍（9 vs 75），看着像回归；但 `--replace=rle,gfx,sprite24,util,path,dlg,rec`
（= 第 32 轮的旧集合，不含 svc）与 `none` 就已经差 93.5% 的像素 —— **分叉在第 33 轮之前就有**。
原因是 autokey 按墙钟投递、转译改变了执行节奏，按键落在不同的帧上就会让整条剧情分叉，于是
同一个帧号根本不是同一个场景。**A/B 判据必须走标准配方**（两次 `none` 之间 0 px，可复现）。

### 33.5 环境坑：沙箱拦 `reg.exe`，以及本轮的构建方式

构建时 `build.ps1` 走 vcvars32.bat 会被安全策略拦住（vcvars 会起 `reg.exe`），而它静默少给了
Windows SDK 的 include / lib，`windows.h` 就找不到了。两条改动：

- `build.ps1`：`VSCMD_VER` 已经存在时（即在开发者命令提示符里）**不再重复调用 vcvars** ——
  环境本来就是对的，重复调用只是多起一次 `reg.exe`。
- 新增 `aux_build.bat`（cmd 包装）：先 `call vcvars32.bat`，再手工补上
  `Windows Kits\10\Include\10.0.26100.0\{ucrt,um,shared}` 与 `Lib\10.0.26100.0\{ucrt,um}\x86`，
  最后调 `build.ps1 -Target <name>`。用法：`cmd //c E:\FD2\port\aux_build.bat typecheck`。
- `regress.ps1` 的第一步要删 `build\sandbox`，同样被安全策略拦（safe-delete fail-closed）。
  本轮改用 **`build\sandbox33`**（新建、不删任何东西）跑同样的流程与断言，不碰 `E:\FD2` 存档。

### 33.6 下轮入口

1. **整体转译 `sub_15F84`**（1380 B 词流解释器，126 个调用点）：它的依赖至此只剩
   `0x15E9E` / `0x15E71` 与上述两条服务，而服务已转译；对拍可用 §25.2 的 CRT 重定向 +
   §33.2 的确定性时钟，按 §32 的做法对比整帧 VGA 与事件序列。
2. **补一条能进"打字进行中"画面的 autokey 配方**：标准配方到帧 1500 时是静态等键态
   （1450 / 1500 / 1560 三帧完全相同），所以本轮 A/B 证明的是"没有回归"，**并没有**证明
   `dlg_type_step` 在宿主里真的跑过 —— 那要等能在打字进行时抓帧。
3. 排期项（不阻塞）：CRT 堆 + 文件层整体替换后，`res.c`、`0x15E71` / `0x15E9E` 一起接入；
   `0x25B45`（`0x25A96` 的同形函数，句柄 `0x53EE8`、闸门 `0x540FF`）可与 `0x25A96` 合并处理。

### 33.7 顺带定位：SFX 的"瞬间刺啦一下"（杂音）

用户反馈"有时候声音会杂音，就是瞬间刺啦一下"。**已验证**的代码路径（`src/ail.c`）：

* `svc_play_sfx` 的开头就是 `AIL_stop_sample(dword_53EE4)`（第 33 轮反汇编 `0x25A96` 已确认），
  即 **每个音效都是"先 stop 再 start"**；
* `host_AIL_stop_sample` → `sample_close_device` → **`waveOutClose`**（`ail.c:689`）；
* `host_AIL_start_sample` → `sample_play` → 再 `sample_close_device`（此时 dev 已 NULL，空转）→
  **`sample_open_device` → `waveOutOpen`**（`ail.c:530/533`）；
* 整段 PCM 只在 `sample_play` 里 `malloc + memcpy + pcm_apply_gain + 一次 waveOutWrite`
  提交，**没有淡入/淡出、没有交叉淡化、没有过零点对齐**（`ail.c:526-563`）。

⇒ **一次 SFX = 一次 `waveOutClose` + 一次 `waveOutOpen`**，而且上一个 buffer 若是正在播放的，
`waveOutClose` 会在**任意非零点**上硬切波形 → 阶跃 → 爆音。第 33 轮宿主实测单局 **51 次 SFX**
（`ail: play` 计数），即约 1.7 次/秒的设备拆装，与"有时候刺啦一下"完全对得上。

**次要嫌疑人**（同类机制，音乐侧）：`synth.c` 的循环用 `Sleep(200)` 轮询 `WHDR_DONE` 再重提交
（`synth.c:413-426`），buffer 结束后最多 200 ms 静音再起 → 每个循环点一次从静音跳到有声的阶跃；
`synth_stop` 也是 `waveOutReset` 硬切（`synth.c:603`）。

**待验证假设**（未实测，需先给 SFX 加一个 WAV dump 才能确认）：FD2 的 PCM 首/尾样本本身是否
不在零（8-bit 的 0x80）上。若不在，即使不拆设备，起停仍会各响一声——那就要靠淡入淡出解决。

**修复方向（按性价比排序，都不改游戏语义）**：

1. **去掉设备拆装**：每个 sample 常驻一个 `waveOut`（首次 play 时打开，`ail_shutdown` 时关），
   重触发改成 `waveOutReset` + 等 `WHDR_DONE`（照抄 `synth_stop` 的 `while (!(hdr.dwFlags &
   WHDR_DONE)) Sleep(10)`）+ 重新 `waveOutWrite`。风险点是"驱动仍持有 buffer"，用双 header
   轮换最稳。
2. **起停加 2~5 ms 斜坡**：在 `pcm_apply_gain` 旁边加一个 ramp（首尾各约 100 样本），把阶跃
   抹平；顺带解决"首/尾不在零点"那条假设。
3. **治本**：走 §13.6 第 3 步 `audio_sokol.c` + `audio.h`，用 sokol_audio 回调做软件混音，
   音乐与 SFX 共用一条流，设备只开一次，斜坡在混音器里统一做。

> **2026-10-06 已按 1 + 2 修完**，实测见 `docs/AUDIO.md` §11.6（20 s 跑 47 次 play、`(cut)` 0 次；
> SFX 实为 667 B @11025 Hz = **60 ms**，不会重叠 ⇒ 主因确认是起停阶跃而非硬切）。
> 第 3 条（软件混音）仍是治本项。

---

## 34. 第 34 轮：抓帧触发改判（帧号 → 墙钟 → guest tick）+ 后端实测（2026-10-06）

**起因**：用户两个问题 —— (a) 为什么调试一直在用 GDI（窗口标题里的 "POC"）而不用 sokol；
(b) 音效偶发"刺啦"一声。(b) 见 §33.7 / `docs/AUDIO.md` §11.6，本轮记 (a)。

### 34.1 先实测：sokol 到底能不能跑

`-Render sokol` **能编能跑**（之前只是没人验）：`src/main_sokol.c` 264 行、
`src/render_sokol.c` 286 行、`src/sokol_impl.c` 28 行，都已入库。真跑结果（同沙箱、
`--exit-after=20`、`--autokey` 走 continue）：

| | `-Render gdi` | `-Render sokol` |
|---|---|---|
| 日志 | `host: render backend = gdi` | `host: render backend = sokol` |
| 图形栈 | StretchDIBits | `sokol: backend=D3D11 image=320x200 … swapchain=960x600` |
| 20 s 帧数 / fps | 637 / **31.9 fps** | 3207 / **159.0 fps** |
| exe | 95,744 B | 287,232 B |

`swap_interval=1` 在无显示/RDP 会话里不生效 ⇒ sokol 不限速，帧数是 GDI 的 **5×**。

### 34.2 改判：验收标准不能用"同帧"

`docs/BACKEND.md` §13.6 第 2 步原来写"**GDI vs sokol 同帧截图逐像素一致**"——这标准本身是错的：
**帧号只有在同一帧率下才是"时刻"**。32 fps 的第 700 帧和 159 fps 的第 700 帧差 20 秒游戏时间。

按用户建议改成按时间：新增 `--shot-time=<ms>`（墙钟，`host_init` 起算）。实测两后端确实落在
同一毫秒量级，但**帧周期 = 采样误差**：

```
host: frame  508 dumped … (age 16016 ms)   -Render gdi    31.8 fps
host: frame 2525 dumped … (age 16000 ms)   -Render sokol 158.3 fps
```

GDI 帧周期 31 ms，"age >= 16000"最多迟到一整个帧周期（实测迟到 **16 ms**），于是比 sokol
**多走一步动画** ⇒ 逐像素差 **10.29%**（6584/64000，差异全部落在动画精灵/文字区域；
用户肉眼判定"差 1 帧，GDI 多向前走 1 步"）。**这恰恰证明高帧率按时间采样更精确**：
GDI 采样精度 ±31 ms，sokol ±6 ms。

### 34.3 再收紧一层：`--shot-tick`

用**游戏自己的时钟**触发：`0x40:0x6C` 的 BIOS tick 由 `dos.c bios_tick_thread` 以 18.2 Hz
独立推进，与帧率无关 ⇒ 同一 tick 计数 = 同一 guest 状态。新增 `host_guest_tick()`（`host.c`）
与 `--shot-tick=<n>`，日志同时打出 `age` 与 `guest tick`。实测：

```
host: frame  554 dumped … (age 17406 ms, guest tick 290)   -Render gdi
host: frame 2730 dumped … (age 17188 ms, guest tick 290)   -Render sokol
```

两者抓在同一个 tick 上（墙钟差 218 ms 只是启动抖动）。**本轮按用户指示停止逐像素对拍**，
两张图留在 `build/tick_gdi.bmp` / `build/tick_sokol.bmp`，需要时一条 `framediff.ps1` 即可验。

### 34.4 顺带修的两个坑

* **抓帧路径必须是绝对路径**：宿主会 `chdir` 到游戏目录，相对路径写不出 BMP 且只在日志里留
  一行 `host: cannot write frame dump …`（排查花了两轮运行）。已写进 `docs/ENVIRONMENT.md`。
* **`aux_build.bat` 原来只透传 target**，加不了 `-Render`；改成透传 `%*`，现在
  `aux_build.bat fd2host -Render sokol` 可用。

### 34.5 结论：GDI 为什么还是默认

不是"高帧率不可比"这个理由（34.2/34.3 已把它推翻），而是：`build.ps1 -Render` 默认 `gdi`；
第 2 步验收没落地；GDI 全进程内单线程便于断点。但**改用 tick 触发后 sokol 反而更适合当
测量平台**（采样更细）。要切：`cmd //c E:\FD2\port\aux_build.bat fd2host -Render sokol`。
