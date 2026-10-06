# FD2 port 文档地图

> 2026-10-06 重整：`PROGRESS.md` 从"什么都往里塞的 2700 行大文档"拆成**按主题分类**的文档，
> `README.md` 退回"项目入口卡片"。两类文档的分工：
>
> | 文档 | 只放什么 |
> |---|---|
> | `README.md` | 目标、路线、目录树、快速开始、当前状态一句话 —— **给第一次看的人** |
> | `PROGRESS.md` | 轮次时间线 + 下一步计划 —— **给想知道"现在到哪了"的人** |
> | `docs/*.md` | 知识（事实/设计）、经验（踩坑）、说明（环境/调试/方法）、明细（轮次病历） |
>
> **§ 编号全部沿用旧 `PROGRESS.md` 编号**，所以外部文件里的 `§8-25`、`§25.2` 之类的引用
> 仍然有效；只是那些小节搬了家，按本表找文件即可。

## 一、自建文档（本项目产出，随代码一起维护）

| 文件 | 类别 | 内容 | 旧 § |
|---|---|---|---|
| `README.md`（根） | 入口 | 项目卡片 + 快速开始 | §1 + 现状摘要 |
| `PROGRESS.md`（根） | 进度 | 轮次时间线 + 下一步计划 | §0 §5 §7 |
| **`ENVIRONMENT.md`** | 说明 | 工具链、构建/运行、**全部命令行参数**、Ghidra HTTP 桥、IDA MCP | §2 §10 |
| **`BINARY-FACTS.md`** | 知识 | `FD2.EXE` 容器/LE 头实测偏移/fixup 格式/对象布局/平台依赖 | §3 |
| **`HOST-DESIGN.md`** | 知识 | 地址空间硬约束、源文件职责、VEH 四类异常、**服务返回值语义表** | §4 |
| **`PITFALLS.md`** | 经验 | **踩坑清单（动手前必读）** + 历史卡点 | §8 §6 |
| **`DEBUG-MANUAL.md`** | 说明 | 诊断手段：自检/对拍 exe/抓帧/崩溃转储/单步/A-B 差分 | §9 |
| **`AUDIO.md`** | 知识 | AIL 入口替换、XMIDI、软件合成器、gm.dls、已知杂音 bug | §11 |
| **`BACKEND.md`** | 说明 | sokol 选型实测、git 策略、跨平台抽取顺序 | §13 |
| **`TRANSLATION.md`** | 知识 | 源码转译方法总览 + 模块/对拍/接入清单 | §19–§33 提炼 |
| **`FDPS-ARCHIVE.md`** | 存档 | 炎龙外传 FDPS（**已冻结**）的成果与卡点 | §14–§18 |
| `rounds/01-platform-and-tooling.md` | 明细 | 平台层文件服务补全、回归提速 | §12 §20 |
| `rounds/02-translation-toolkit.md` | 明细 | RLE、图形 blit、sprite24、字节/调色板工具 | §19 §21 §22 §23 |
| `rounds/03-tables-and-plumbing.md` | 明细 | 寻路、资源加载器、**CRT 重定向**、转译接入、表访问器 | §24 §25 §26 §27 |
| `rounds/04-dialog-and-ui.md` | 明细 | 0xC0-RLE 文本、对话框、开收框动画、等键 | §28 §29 §30 §31 |
| `rounds/05-rec-and-services.md` | 明细 | 角色记录表、打字机步进 + tick 等待 + PCM 音效 | §32 §33 §34 |
| `rounds/06-audio-fade.md` | 明细 | 场景/进游戏音量“先小后大”排查（原作淡入）+ 起播抢跑修复 | §35 |
| `rounds/07-sokol-acceptance.md` | 明细 | sokol 与 GDI 同 guest tick 逐像素验收 + 取样点规则（先验基线） | §36 |
| `rounds/08-svc-sfx2.md` | 明细 | `0x25B45` 同形函数转译接入 + `sub_15F84` ABI 测绘纠正 | §37 |
| `rounds/09-vm.md` | 明细 | **脚本 VM `0x15F84` 转译**（词流解释器、共享尾声、EDI 语义、5512 例对拍） | §39 |
| `rounds/10-typewriter-recipe.md` | 明细 | “打字进行中”autokey 配方 + 逐字证据 + 抓图快流程（抓完即退）与 BMP→PNG 正确写法 | §40 |
| `rounds/11-audio-mixer.md` | 明细 | **音频治本**：`audio.h` + `audio_sokol.c` 软件混音器（一个设备）、`--audio-dump` 可测判据 | §41 |
| `rounds/12-keylog.md` | 明细 | **按键录制/回放**（`--keylog`/`--keyplay`，独立模块 `src/keylog.c`）+ 启动抢焦点混入杂键 | §42 |

逆向侧另有两份：**`re/RE_MAP.md`**（FD2 测绘/函数分区/转译路线）、
**`re/FDPS_MAP.md`**（FDPS 测绘，随 FDPS 一并冻结）。

## 二、外部参考资料（`fd2_re` 快照，**不是本项目产出**）

`docs/knowledge-base/` + `docs/data/` + `docs/KEEP.md` 是上游
[`github.com/wicanr2/fd2_re`](https://github.com/wicanr2/fd2_re)（Go/Ebiten **重制**）的 docs 快照。

- **只作语义参考**：上游分析的是**另一个 FD2.EXE build**（md5 `b97caf22…`，本项目 `a6e341a8…`）；
  地址/常量/指令一律以本项目的 IDA 库 `E:\FD2\FD2.EXE.i64` 为准。
- 取舍理由与保留清单见 `docs/KEEP.md`；评估结论见 `rounds/02-translation-toolkit.md` §21.1。
- 它是外部快照，不随本 port 仓库提交（`.gitignore` 只忽略它，不忽略上面第一节的自建文档）。

## 三、按"我要做什么"查表

| 我要… | 去哪看 |
|---|---|
| 改 `src/le.c` / 换个 LE 游戏 | `BINARY-FACTS.md` + `PITFALLS.md` + `re/preflight.py` |
| 改 `src/dos.c`（服务语义） | `HOST-DESIGN.md` §4.4 语义表 + `PITFALLS.md` |
| 改 `src/ail.c` / 声音有杂音 | `AUDIO.md`（§33.7 有杂音定位与修复方向） |
| 做源码转译（新模块） | `TRANSLATION.md` 方法篇 + 最近一轮 `rounds/05-…` 的"下轮入口" |
| 调不动/跑飞/画面错 | `DEBUG-MANUAL.md` → 再看 `PITFALLS.md` 对应条目 |
| 建环境/忘参数 | `ENVIRONMENT.md` |
| 想知道还能做什么 | `PROGRESS.md` 的"下一步计划" |
