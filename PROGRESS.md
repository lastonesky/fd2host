# FD2 → Windows 原生移植 — 进度存档

> **本文件只放"进度"**：一句话现状、已达成的能力清单、轮次时间线、下一步计划。
> 每轮的完整病历（为什么这么改、踩了什么、判据是什么）在 `docs/rounds/*.md`；
> 知识/经验/说明类内容在 `docs/*.md`。总导航见 **`docs/INDEX.md`**，
> 其中有旧 `PROGRESS.md` 的 §编号 → 新文件对照表（§ 编号全部沿用，外部引用不需要改号）。

---

## 0. 一句话现状

32 位宿主 `port/build/fd2host.exe` 能把 DOS/4GW 的 `FD2.EXE` 加载进 Win32 进程、接管它全部的
`int`/端口/低地址访问，让**原始 x86 游戏代码原生执行**：320×200×256 视频模式、按 DAT 偏移表加载
资源、**持续渲染开场动画**、**有声音**（音效 waveOut + XMIDI 自带合成器），并能用 `--autokey`
自动走完"片头 → 标题菜单 → continue → 剧情画面"。

**当前重心是路线 C 的主体：逐步源码化。** 已把 49 个函数从机器码还原成 C、经 `src/repl.c`
接入运行中的游戏（逐字节对拍 + `regress.ps1` 8/8），下一个目标是 `sub_15F84` 脚本 VM。
方法总览见 **`docs/TRANSLATION.md`**。

## 1. 任务与路线

- **目标**：`E:\FD2\FD2.EXE`（DOS/4GW + Watcom + Miles AIL 的 32 位保护模式游戏）
  在 Windows 上**原生**运行。**不使用** DOSBox、不模拟 DOS、不模拟实模式 CPU。
- **路线 C（已选定）**：先做 32 位"二进制宿主"——保留原始 x86 游戏逻辑，平台层换成现代实现；
  之后逐函数换成还原的 C 源码，最终产出**可编译的高级语言引擎**。
- **每轮判据**：必须以硬证据收尾（`letest` 逐字节 / `*check` 用例数 / `host.log` 行 /
  抓帧差分 / `regress` 8/8）。禁止"字节扫描、听起来像"式结论，未证实的一律标"待确认"。

## 2. 已达成的能力清单（`build/host.log` 实证）

```
LE 加载 + 7937 条 fixup 应用                    ✅
VEH 安装，int 站点改写为 int3 并接管              ✅
进入游戏入口 0x3CCB4 → CRT 初始化 → AIL 探测      ✅
INT 10h AH=0 设 0x13 模式、调色板端口 I/O         ✅
按 DAT 偏移表加载资源、RLE 解压到帧缓冲            ✅（修 AH=42/48 语义之后）
开场动画持续渲染：30 s / 960 帧 / 无崩溃           ✅ ← POC"看到画面"达成
声音：AIL 16 入口替换 + XMIDI 合成 + gm.dls 音色   ✅
键盘：INT 16h 菜单导航 + BDA 轮询                  ✅（鼠标经静态+运行期双证不需要）
文件服务：AH=3C 创建 / AH=41 删除 / AH=40 截断      ✅ fresh install 不再崩，regress 8/8
游戏退出路径（INT10 mode 3 → AH=4Ch → shutdown）   ✅
第 1 步接口抽取：render.h / host.h / main_win32.c  ✅ GDI 成为第一个后端
源码转译：**49 个函数接入运行中的游戏**             ✅ 详见 docs/TRANSLATION.md
```

## 3. 轮次时间线

| 轮次 | 日期 | 主题 | 成果一句话 | 明细 |
|---|---|---|---|---|
| §9/§11 | 10-04 | POC | 色偏与特权指令长度修正、AIL 替换层 + 音乐打通 | `docs/PITFALLS.md`、`docs/AUDIO.md` |
| §12 | 10-05 | 平台层文件服务 | 补 `AH=3C/41` + `AH=40 CX=0` 截断；静态证明**游戏不用鼠标** | `docs/rounds/01-platform-and-tooling.md` |
| §13 | 10-05 | 后端选型 | 实测 SDL2/SDL3/sokol ⇒ **选 sokol**（0 DLL）；抽出 render/host/入口层 | `docs/BACKEND.md` |
| §14 | 10-05 | 通用化 + FDPS 首跑 | `--exe` 可跑任意 LE 游戏；FDPS 120 s 无崩溃 | `docs/FDPS-ARCHIVE.md`（冻结） |
| §15 | 10-05 | FDPS AIL 表 + 定时器 | 90 条入口 + 1 ms tick 宿主定时器线程，动画时钟精确 25 Hz | `docs/FDPS-ARCHIVE.md`（冻结） |
| §16 | 10-05 | `AH=4B`(EXEC) | 真拉起子进程跑 FD.EXE，命令尾巴 19 字节逐字节正确 | `docs/FDPS-ARCHIVE.md`（冻结） |
| §17 | 10-05 | FD.EXE 调研（只读） | FD.EXE = 过场播放器；父进程不响应的根因是自挂 INT 9 | `docs/FDPS-ARCHIVE.md`（冻结） |
| §18 | 10-05 | INT 9 投递 | 在 guest 线程压中断帧；揪出 `type 0x02` fixup 写宽度 bug | `docs/FDPS-ARCHIVE.md`（冻结） |
| §19 | 10-05 | **RLE 转译** | `0x4E98D`/`0x4E8D3` → `game/rle.c`，**建立机器码对拍法**（1900 例） | `docs/rounds/02-translation-toolkit.md` |
| §20 | 10-05 | 回归提速 | `--exit-when-file` + 轮询退出 ⇒ **75 s → 15 s**，环境性崩溃自动重试 | `docs/rounds/01-platform-and-tooling.md` |
| §21 | 10-05 | 图形 blit 族 | `0x4ECBF`…`0x4EEE0` → `game/gfx.c`（1450 例）；评估 `fd2_re` 知识库 | `docs/rounds/02-translation-toolkit.md` |
| §22 | 10-05 | sprite24 族 | 7 个颜色模式变体 → `game/sprite24.c`（2100 例）；docs 快照清理 | `docs/rounds/02-translation-toolkit.md` |
| §23 | 10-05 | 字节/调色板 | 6 函数 → `game/util.c`（2200 例）；发现不守 ABI 与返回值语义两坑 | `docs/rounds/02-translation-toolkit.md` |
| §24 | 10-05 | 寻路簇 | `0x4E390..0x4E751` → `game/path.c`（1000 例） | `docs/rounds/03-tables-and-plumbing.md` |
| §25 | 10-05 | 资源加载器 | `0x111BA` → `game/res.c`（160 例）；**CRT 重定向对拍术** | `docs/rounds/03-tables-and-plumbing.md` |
| §26 | 10-05 | 转译接入 | 新增 `src/repl.c`，23 个函数**真的在跑 C**；A/B 固定帧 0 px 差 | `docs/rounds/03-tables-and-plumbing.md` |
| §27 | 10-05 | 表访问器 | 11 个 → `game/tables.c`（4528 例），接入 23→34，obj0 工具库清零 | `docs/rounds/03-tables-and-plumbing.md` |
| §28 | 10-05 | 0xC0-RLE 文本 | `game/rle2.c`（1200 例），接入 34→37 | `docs/rounds/04-dialog-and-ui.md` |
| §29 | 10-05 | 对话框辅助 | `game/dlg.c`（800 例，整帧 VGA），接入 37→39 | `docs/rounds/04-dialog-and-ui.md` |
| §30 | 10-05 | 开框/收框动画 | app-level 写法确立；事件序列对拍（240 例），接入 39→43 | `docs/rounds/04-dialog-and-ui.md` |
| §31 | 10-05 | 等键 + 动画 | `0x16C57` 低内存镜像 + 确定性时钟（100 例），接入 43→44；`--volume` | `docs/rounds/04-dialog-and-ui.md` |
| §32 | 10-05 | 角色记录表 | `rec_flag`/`rec_find` → `game/rec.c`（28739 例），接入 44→46 | `docs/rounds/05-rec-and-services.md` |
| §33 | 10-06 | 打字机 + 系统服务 | `svc.c` + `dlg_type_step`（1176 例，确定性时钟 + 变异验证），接入 46→**49**；顺带定位 SFX 爆音 | `docs/rounds/05-rec-and-services.md` |
| §34 | 10-06 | 抓帧触发改判 + sokol 实测 | sokol **能编能跑**（D3D11 / 159 fps）；验收标准“同帧”是错的 → 加 `--shot-time`，再收紧为 **`--shot-tick`**（按游戏 BIOS tick，跨后端同状态）；SFX 爆音按“常驻设备 + 3 ms 起停斜坡”修完 | `docs/rounds/05-rec-and-services.md` §34、`docs/BACKEND.md` §13.8、`docs/AUDIO.md` §11.6 |
| §35 | 10-06 | 音量“先小后大”排查 | **不是回归**：是游戏 `play_bgm` 自己的 `set(0,0)+set(127,2000)` 2 秒淡入，原版 AIL（`sub_449E0`/`sub_43270`/`sub_42980`）照实现，15 首曲子全声道有 CC7 ⇒ 覆盖等价；顺带修掉移植侧**起播 371 ms 抢跑旧增益**（起播闸门，`gain 0.000` 判据） | `docs/rounds/06-audio-fade.md`、`docs/AUDIO.md` §11.9、`docs/PITFALLS.md` §8-52 |

## 4. 下一步计划（按优先级）

1. **源码化继续**：`sub_15F84` 脚本 VM（1380 B 词流解释器，126 个调用点）——依赖只剩两条已转译
   的服务（`svc_wait_ticks`/`svc_play_sfx`）与 `0x15E9E`/`0x15E71`（仍留原机器码）；
   用 CRT 重定向 + 确定性时钟对拍。**详见 `docs/TRANSLATION.md` §5。**
   已完成：资源加载器 ✅、RLE/blit ✅、obj0 工具库 ✅、对话框系列 ✅、角色记录 ✅、系统服务 ✅。
2. **补 autokey 配方**：当前标准配方到帧 1500 是静态等键态，只证明"无回归"，没证明
   `dlg_type_step` 在宿主里真的跑过（需要能进"打字进行中"画面的按键序列）。
3. **显示层换 sokol**：代码已完成且实测可跑（§34），剩下的是**验收**——标准已从"同帧"改判为
   **同一 guest tick**（`--shot-tick`），两张对照 BMP 已生成，跑一条 `framediff.ps1` 即可收口；
   `--render=gdi` 继续留作调试基准。详见 `docs/BACKEND.md` §13.8。
4. **音频治本**：SFX 爆音已按"常驻设备 + 3 ms 起停斜坡"修完（`docs/AUDIO.md` §11.6）；
   剩下的是**软件混音**（sokol_audio 统一音乐 + 音效、设备只开一次），它同时消掉音乐循环点的
   `Sleep(200)` 静音阶跃与重触发硬切。
5. **稳定性长跑**：连续 5 分钟以上与反复重启（退出路径已验）。
6. **存档路径实测**：`FD2.SAV` 从无到有的创建路径、存档变小后的截断对拍；
   `AH=49/4A` 仍是空操作（账本只增不减）。
7. **跨平台**：单代码库 + 后端选择（**不用 git 分支**），先 Linux x86-64；顺序见 `docs/BACKEND.md`。
8. ~~FDPS（炎龙外传）~~ **已冻结**（2026-10-05 用户决定）：成果与卡点存档在 `docs/FDPS-ARCHIVE.md`，
   宿主的通用能力（`--exe`、FDPS AIL 表、定时器线程、INT9 注入）留在代码里不再主动维护。

## 5. 冻结 / 不再投入

| 项 | 状态 | 存档 |
|---|---|---|
| FDPS（炎龙外传） | 冻结（2026-10-05 用户决定） | `docs/FDPS-ARCHIVE.md` |
| SDL2 / SDL3 | 降为备选记录 | `docs/BACKEND.md` §13.2 / §13.3 |
| ~~鼠标 `INT 33h`~~ | 已判定不需要 | `docs/rounds/01-platform-and-tooling.md` §12.3 |
