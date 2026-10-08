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
| `rounds/13-portability.md` | 明细 | **跨平台第 1 刀**：`platform.h` 内存层、`le.c` 零 Win32、Win32/Linux 哈希一致、全仓 Win32 依赖面测绘 | §43 |
| `rounds/14-fixup-boundary.md` | 明细 | **跨页 fixup 两类之辨**：补回 11 处漏掉的重定位，三对象与 Ghidra 逐字节一致 | §44 |
| `rounds/15-dos-and-faults.md` | 明细 | **跨平台第 2 刀**：`dos_fault.h`（VEH/sigaction 薄包装 + 可移植核心）、`dos_ctx`、platform.h 第 2 切片（`pread/pwrite`）、`faultprobe32` 实测 i386 compat 故障模型、`doscheck` 两平台 49/49 | §45 |
| `rounds/16-entry-layer.md` | 明细 | **跨平台第 3 刀**：便携键表 `keys.c` + X11/XWayland 判定、`host.c`/`keylog.c` 过河、Linux 宿主 `host32` | §46 |
| `rounds/17-res-and-guest-heap.md` | 明细 | `res.c` 接入 + **`guest_mem` 唯一堆缝**（游戏堆 / 宿主 malloc） | §47 |
| `rounds/18-snapshot-pair.md` | 明细 | 快照对 `0x15E9E`/`0x15E71` → `dlg_snap_save/restore`（走 guest_mem） | §48 |
| `rounds/19-play-bgm.md` | 明细 | 主状态机族第一刀：换曲入口 `play_bgm`（`movzx+cmp` 不能写成 `(uint8_t)` 比较） | §49 |
| `rounds/20-scene-card.md` | 明细 | `scene_card` 场景卡 + 钉死状态机分派表 `funcs_25E23/25E3A` | §50 |
| `rounds/21-fade.md` | 明细 | 调色板淡变三件套 `0x11D40`/`0x1F882`/`0x1F525` | §51 |
| `rounds/22-hot-leaves.md` | 明细 | 热叶子六件套（kbd/util/gfx/res/dlg）+ `rle_decode` 目的地偏移坑 | §52 |
| `rounds/23-map-view.md` | 明细 | 地图视图叶子四件套 `map_blit_tile`/`pal_fade_add`/`res_blit6`/`dlg_portrait_clear` | §53 |
| `rounds/24-anim-cell.md` | 明细 | 动画/地图格/头像查找四件套（BDA tick 帧计数、格信息、头像查找、表访问器） | §54 |
| `rounds/25-number-render.md` | 明细 | 数字渲染链 `0x187D6`/`0x1875D`/`0x1AEB1` | §55 |
| `rounds/26-cell-sprites.md` | 明细 | 格子对象精灵链 `0x1F183`/`0x12AC6`/`0x129EC` | §56 |
| `rounds/27-portrait-draw.md` | 明细 | 头像精灵链 `0x127E0`/`0x127A9`（32 位偏移表 `*(0x53A61)`）+ harness 全局值域坑 | §57 |
| `rounds/28-rec-slots.md` | 明细 | 角色记录 8 槽字段访问器簇 `0x1B722`/`0x344F2`/`0x1BB8C`/`0x1B8E7` + 残留进程锁构建产物坑 | §58 |
| `rounds/29-unit-roster.md` | 明细 | 持久队伍记录表三件套 `0x1145A`/`0x11506`/`0x112A5`（闭合 `funcs_25E23` 5 表项）+ A/B 需 autokey 静止窗 | §59 |
| `rounds/30-scene-states.md` | 明细 | 主状态机转移 handler 五件套 `0x22EF6`/`0x231BC`/`0x23790`/`0x2389F`/`0x23E39`（`funcs_25E23` 5 表项）+ `--shot-tick=500` 需 >30 s | §60 |
| `rounds/31-scene-state-14.md` | 明细 | `funcs_25E23[14]` handler `0x239BD` + 队伍身份查询叶子 `0x33499`（闭合第 6 个表项） | §61 |
| `rounds/32-fx-handlers.md` | 明细 | `funcs_30469` 效果动画 handler 四件套 `0x2C217`/`0x2CAFC`/`0x2CCF4`/`0x2CE1A`（新模块 `fx.c`，新分组 `REPL_FX`）+ 栈探针伪像藏立即数 | §62 |
| `rounds/33-fx-tail.md` | 明细 | `funcs_30469` 收尾 5 handler + 辅助 `0x2B996`/`0x2BB33`/`0x2BD6C`+`0x2BF83`/`0x2BFD9`/`0x2C441`（整表 9/10）+ `fx_advance` 资源块直测 + A/B 需 autokey 配方 | §63 |
| `rounds/34-rec-leaves.md` | 明细 | 角色记录表七个数据叶子 `0x1B8A6`/`0x1B83D`/`0x1CA89`/`0x13512`/`0x32975`/`0x34D64`/`0x35009`（105–111 个；**6 个返回值语义只有 1 个是记录地址**、`0x1CA89` 16 位回绕）+ `repl_parse` 漏 `map` 组名（§8-77）+ 新跨模块调用只在链接期暴露（§8-78） | §64 |
| `rounds/35-msg-portrait.md` | 明细 | 对话框/头像合成三件套 `0x1956B`/`0x1974C`/`0x26996`（112–114 个；新模块 `game/msg.c`，第 28 轮后置的 `guest_mem` 堆簇；`msgcheck` 465/0） | §65 |
| `rounds/36-ev-handlers.md` | 明细 | `funcs_1199C` 事件 handler 闭包子集 `0x34738`…`0x35258`（115–125 个；新模块 `game/ev.c`，6 服务记录桩 + 记录表逐字节 + 归一化返回值，`evcheck` 2940/0）+ `_chkstk` 保持 EAX ⇒ 部分返回值无定义（§8-79） | §66 |
| `rounds/37-map-view-core.md` | 明细 | 地图视图渲染核 `0x11EEE`/`0x24D22`/`0x122DC`/`0x1ACF3`（126–129 个；并入 `game/map.c`，CRT `malloc/memmove/free` 重定向 + 三块缓冲逐字节 + 6 相位全局，`mapcheck` 112000/0；关键手法 `apply_tinfo` 回写真 cdecl 原型再反编译，§8-80） | §67 |
| `rounds/38-palette-and-map-refresh.md` | 明细 | 调色板动画 + 地图视图刷新 `0x4E310`/`0x4E31C`/`0x32230`/`0x11CAC`（130–133 个；`fade.c`/`map.c`，窄 VEH `out` 陷阱 + 事件序列/整块 VGA 逐字节，`mapcheck` 122500/0；新蹈坑：非关键 VGA 块未提交致间歇 AV（§8-83）、域外 `t[k-1]` 不可复现（§8-84）） | §68 |
| `rounds/39-funcs1199c-batch2.md` | 明细 | **批量转译工作流 + `funcs_1199C` 第二批 30 个**（134–163 个；发现该表是 **91 项**，不是 48；`0x35298..0x3644E` 场景脚本簇 + `0x135DD` → 新模块 `game/ev2.c`、新分组 `REPL_EV2`；`ev2check` **支持 `--only=` 子集**、6 批×1000 例全过；`--replace` 取反 + `FD2_REPL_SKIP` 免重建二分；新蹈坑：内部 helper 撞 `vm_run` 使 `translation_map` 归属错（§8-86）、偶发 regress（§8-85）） | §69 |
| `rounds/40-funcs1199c-batch3.md` | 明细 | **`funcs_1199C` 场景脚本簇收口**（164–191 个；剩余 23 表项 + 5 helper `0x35B78`/`0x35F10`/`0x361B0`/`0x2AEDB`/`0x33F78` → `game/ev3.c`、`REPL_EV3`；`ev2check` 扩 18 桩 + **整个 obj2** 快照；抓到 `0x35E5B` 漏尾部 `vm_run(6)`（共享尾落在函数中间，§8-87）与 harness 未恢复 obj1/obj2 起点（§8-88）） | §70 |

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
