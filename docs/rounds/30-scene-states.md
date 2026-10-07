# 轮次明细：主状态机转移 handler 五件套（第 89–93 个转译函数）

§60 按规划 agent 的分配，把 `main`（`0x25BF4`）主循环 `call funcs_25E23[dword_53C03]`
（转移分派表 `funcs_25E23 @0x51DE9`）的 5 个表项还原进既有 `src/game/scene.c`：
`0x22EF6`(`[0]`)、`0x231BC`(`[3]`)、`0x23790`(`[10]`)、`0x2389F`(`[12]`)、`0x23E39`(`[18]`)。
合计 **315 B / 23 个调用点 / 5 个表项指针**，每个都只是 `vm_run` + `unit_refresh_all`
（+ `unit_add`）再改 `dword_53C03`，纯服务序列（零 VGA 直写、零堆、零 I/O）。
扩进既有 `scenecheck`，接入既有分组 `REPL_SCENE`。`scenecheck` **100 → 1100/0**；
`regress` **8/8**、`repl: installed 93`、静态帧 A/B **0 px**。

---

## 60.1 这一批

| addr | size | usage | 表项 | C 名 | 语义 |
|---|---|---|---|---|---|
| `0x22EF6` | 65 | 7 | `[0]` | `scene_state_00` | `vm_run(流,9,…)` → `unit_refresh_all` → `dword_53C03 = 1` |
| `0x231BC` | 61 | 4 | `[3]` | `scene_state_03` | `vm_run(流,4,…)` → `unit_refresh_all` → `dword_53C03++` |
| `0x23790` | 69 | 4 | `[10]` | `scene_state_10` | `vm_run(流,3,…)` → `unit_refresh_all` → `unit_add(14)` → `dword_53C03++` |
| `0x2389F` | 61 | 4 | `[12]` | `scene_state_12` | `vm_run(流,9,…)` → `unit_refresh_all` → `unit_add(3)` → `dword_53C03++` |
| `0x23E39` | 59 | 4 | `[18]` | `scene_state_18` | `unit_refresh_all` → `vm_run(流,3,…)` → `dword_53C03++` |

三个被调全部**已接入**（`vm_run` `REPL_VM`、`unit_refresh_all`/`unit_add` `REPL_REC`），
其余只有 Watcom 栈探针 `0x3702F`（CRT，约定不转译）⇒ **依赖完全闭合**。转完即闭合
`main` 主循环转移表唯一能干净拿下的一批表项（其余 20 项各自依赖尚未转译，见 `rounds/29` §59.5）。

### 60.1.1 IDA 逐条核对（`E:\FD2\FD2.EXE.i64`）

5 个函数都是 `push 28h; call 0x3702F`（栈探针）→ 9 个 `push` → `call 0x15F84` →
`add esp,24h`。`vm_run(stream, sub, addr, pitch, fg, shadow, bgfill, line_step, wait)`
的 9 个实参**逐字节相同**（`1, 0x13, 0x4A, 0x4C, 0xCD, 0x140, 0xA0000, <sub>, dword_53A79`），
只有 `sub` 不同（3/4/9）。`Hex-Rays` 报的 `__usercall … @<eax>…` 是栈探针伪像，真实
ABI = `void fn(void)`（`rounds/08` §37.3，与 `vm.c`/`unit.c` 同一现象）。

三个易错点（差分对拍钉住）：

- **`0x22EF6` 是赋值 `mov dword_53C03,1`，不是 `++`**；另 4 个才是 `inc dword_53C03`。
- **共享尾跳**：`0x23790` 落到 `0x237C8`（`call unit_add; add esp,4; jmp 0x231F2`），
  `0x2389F` 直接 `push 3; jmp 0x237C8`，`0x23E39` 也 `jmp 0x231F2`（`inc dword_53C03; retn`）。
  C **按语义各写各的**，不复刻 jump 布局；`unit_add` 的返回值一律丢弃。
- **`0x23E39` 顺序相反**：`unit_refresh_all` 在前、`vm_run(…,3,…)` 在后；事件对拍能抓到顺序。

## 60.2 对拍（`scenecheck` 扩展）

沿用既有骨架（`le_reserve_address_space` → `le_open` → `le_map_and_relocate`；`install_hook`
5 字节 `jmp`；事件日志逐调用比较）与既有 `scene_card` 100 例。本簇新增 3 个记录桩：

| hook | 记录 |
|---|---|
| `0x15F84` `stub_vm` | 9 个实参原样（`stream` 证明读的是 `dword_53A79`，`sub` 区分 handler），返回 `addr` |
| `0x11506` `stub_unit_refresh` | 一个 `unit_refresh_all` 事件 |
| `0x112A5` `stub_unit_add` | 实参 `id` |

每个用例跑**两份**（原机器码 vs C），判据三件套：

1. **事件序列**：次数 + 每个事件的 9 个实参逐项相等（含 `=1` vs `++` 后的 `dword_53C03` 两处）；
2. **`dword_53C03` 终值**（含补码边界）——由 `0x53A00..0x53E00` 的 1 KiB 全局快照逐字节比出来；
3. **1 KiB 数据段快照**逐字节相等 ⇒ C 没有多写任何全局（快照窗口同时覆盖
   `dword_53A79`@`0x53A79` 与 `dword_53C03`@`0x53C03`）。

输入合成：`dword_53A79` 取 `0 / 0x12345678 / 0xDEADBEEF`，`dword_53C03` 初值取
`0/1/7/0x7FFFFFFF/-1`；5 handler × 200 次重复（覆盖哨兵网格 + 可重入）= **1000 例**
再加 `scene_card` 100 例 = **1100 例 0 failure**。

**判据**：`scenecheck` **100 → 1100/0**（`0x22EF6` 的 `=1` 与 `++` 在初值 7 下可区分；
`0x7FFFFFFF` 的 `inc` 回绕到 `0x80000000` 也逐字节一致）。

## 60.3 一起踩的坑

- **手搓抓图别把 `--exit-after` 设成 30**：BIOS tick 由 `bios_tick_thread` 以 18.2 Hz 推进，
  `--shot-tick=500` 实测在**进程启动后 ~31 s** 才到（`frame dumped … age 31187 ms, guest tick 500`），
  30 s 上限会先触发 watchdog ⇒ 永远抓不到图、`--exit-when-file` 也不触发。用既有
  `build/ab_run.ps1`（`--exit-after=60`）即可。见 `docs/PITFALLS.md` §8-73。
- **`ab_run.ps1` 的 `-WorkingDirectory` 是摆设**：宿主 `--gamedir` 默认硬编码 `E:\FD2`
  （`src/host.c`），脚本删的是 `build/sandbox/FD2.TMP`、实际跑的是 `E:\FD2`。A/B 两侧
  状态相同所以判据成立；`E:\FD2\FD2.SAV` 的 mtime 实测未变（continue 只读存档）。

## 60.4 判据

| 项 | 结果 |
|---|---|
| `scenecheck` | **1100/0**（100 → +1000） |
| `regress.ps1` | **8/8 PASS**、`repl: installed 93`、`FD2.TMP=207360` |
| A/B 同 tick `--shot-tick=500` | `none↔none2` **0 / 64000 px**、`none↔all` **0 / 64000 px** |
| Linux | `make -f Makefile.linux` 构建通过（`scene.c` 0 warning）、`letest-linux` 三对象 exact match、`doscheck-linux` **49/49** |
| `translation_map` / `func_ranking` | **93 wired / 1359（6.8%）**（`--check` 通过） |

进度 **93 / 1359（6.8%）**。

## 60.5 下一步

`funcs_25E23` 分派表 25 项里已完成 5 项；其余 20 项（`0x22F37`/`0x230F2`/`0x231F9`/`0x23296`/
`0x232E8`/`0x234BB`/`0x235BC`/`0x235F9`/`0x237D5`/`0x238DC`/`0x239BD`/`0x23A0A`/`0x23B5F`/
`0x23CD5`/`0x23E74`/`0x240FA`/`0x244B6`/`0x24754`/`0x24C1E`/`0x24DF2`）依赖尚未转译，
需先闭各自闭包（例如 `0x230F2` 缺 `0x11CAC/0x11EEE/0x122DC/0x13536/0x1ACF3/0x233C6/0x24D22/0x4E310/0x4E31C`）。
并行可进的同族叶子（`rec.c` + `reccheck`）：`0x1CA89`(依赖已接入 `0x4E866`)、
`0x1B8A6`、`0x13512`、`0x34D64`（`rounds/29` §59.5）。
