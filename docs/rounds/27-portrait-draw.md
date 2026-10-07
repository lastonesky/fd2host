# 轮次明细：头像精灵链（第 80–81 个转译函数）

§57 收 `0x127A9` 的最后一块拼图 `0x127E0`（头像/图标精灵绘制），再转它本身。
`mapcheck` 扩到 **97000/0**；`regress` **8/8**、`repl: installed 81`、静态帧 A/B **0 px**。

---

## 57.1 这一批

| addr | 用法 | 转译 | 语义 |
|---|---|---|---|
| `0x127E0` | 11 | `dlg_portrait_draw(idx)` | 画记录 `idx` 的 24×24 头像/图标；资源库 `*(0x53A61)`（32 位偏移表在基址），索引 `mode + 12*p[2] + 3*p[3]`；每换一个 BIOS tick 翻转 `dword_53A04` 一次当两帧抖动；`p[5]` bit7 走 `sprite24_ramp24`，否则 `sprite24_plain` |
| `0x127A9` | 20 | `dlg_portraits_refresh(void)` | 对每条 `rec_flag(i)==0` 的记录画头像，最后 `map_refresh_records()` 刷新格精灵 |

两个都进 `game/dlg.c`（`dlg.h` 声明）。`0x127E0` 的字段语义以机器码为准：
`p[0..1]`=格坐标、`p[2]`=图标资源号、`p[3]`=方向/口型（0→+1824、1→-4、2→-1824、其它→+4 的
`step`）、`p[4]`=帧（同时选 `dword_53C07`/`dword_53C0B` 作 `mode`）、`p[5]` bit7=着色模式、
`p[38]`=翻转变体（`mode` 强制 0 且偏移加 `dword_53A04`）。`mode==3` 折叠成 1。

`0x127E0` 的 tick 读法与 `anim.c` 一致（原始是 `movsx eax, word ptr [0x46C]`）：这里走
`ORIG_TICK()`（`0x4E310`，经低内存镜像）再 `(int16_t)` 符号扩展，和 `dlg_wait_key` 用同一条路。
`0x127A9` 直接调已转译的 `map_refresh_records()`。

## 57.2 对拍（`mapcheck` 扩展）

合成 `*(0x53A61)` 头像库：基址 48 项 32 位偏移表 + 4 资源 × 12 帧的 24×24 全字面流
（索引上限 `2 + 12*3 + 3*3 = 47`）。用例：

| 函数 | 判据 |
|---|---|
| `dlg_portrait_draw` | 随机记录（x/y 覆盖视内视外、`p[2]∈0..3`、`p[3]∈0..3`、`p[4]∈0..1`、`p[5]` 两路、`p[38]∈0..1`）→ **整幅位图**逐字节一致，且 `dword_53A04`/`dword_53A08` 副作用一致 |
| `dlg_portraits_refresh` | 随机多条记录（含 `rec_flag` 过滤）+ 低内存 tick → 整幅位图一致（这条同时验证 C 版 `dlg_portrait_draw` 与 C `map_refresh_records`） |

结果 **97000 cases, 0 failures**（173→194 每轮 × 500）。踩坑记 §57.3 与 `PITFALLS` §8-71。

## 57.3 一起踩的坑

- harness 一开始把 `dword_53A04` 当成任意 `int32` 随机（`rnd()` ~ 24 bit），原机器码
  `add ebx, dword_53A04` 后只判 `<0` 就写，直接把精灵写到 `bitmap + ~16 MB` → 段错误，
  看着像 C 转译写飞。游戏里该全局只会被 `xor byte,1` 改成 **0/1**，照值域给随机值即可
  （`PITFALLS` §8-71）。
- 静态帧 A/B 用 `--shot-tick=500`（§8-55 的 380..590 静止窗）得 **0/64000**；
  tick600 落在台词打字/转场段，`--replace=none` vs `all` 会差 31 px（14×4 一块），
  `none↔none2` 自比 0 px ⇒ 是采样点问题，不是接入差异（§8-55 的执行细则）。

## 57.4 判据

| 项 | 结果 |
|---|---|
| `mapcheck` | **97000/0**（86500→+4500） |
| `dlgcheck` / `boxcheck` / `keycheck` / `typecheck` / `leafcheck` | 800 / 240 / 100 / 1616 / 72008 全 0（dlg.c 新增 map.c+sprite24.c 依赖后重建） |
| `regress.ps1` | **8/8**、`repl: installed 81`、`FD2.TMP=207360` |
| A/B 同 tick `--shot-tick=500` | `none↔none2` 0 px、`none↔all` **0 / 64000 px** |
| Linux | `make -f Makefile.linux` 构建通过、`letest-linux` 三对象 exact match、`doscheck-linux` **49/49** |

进度 **81 / 1359（6.0%）**。

## 57.5 下一步

`0x127A9` 的调用图（`0x11cf5`…）里还有 `0x122DC`(1051)、`0x24D22`、`0x4E31C`(101)、
`0x11EEE`(885)、`0x11CAC`(84)、`0x135DD`(98)、`0x1366A`(110) 等；`re/func_ranking.csv` 里
`0x11CAC`(84) 已是依赖闭合的下一刀（`0x10B4E`(58) 是 `0x127A9` 上游的一个入口，也可先收）。
