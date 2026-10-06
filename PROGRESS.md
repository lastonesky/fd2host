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

**当前重心是路线 C 的主体：逐步源码化。** 已把 51 个函数从机器码还原成 C、经 `src/repl.c`
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
源码转译：**51 个函数接入运行中的游戏**             ✅ 详见 docs/TRANSLATION.md
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
| §36 | 10-06 | sokol 显示层验收 | 同 tick **基线 0 px**、GDI vs sokol **31 px（0.0484%）**且全在一块 14×4 动画元素上 ⇒ **第 2 步收口**；取样点必须选静止画面（片头转场同后端自比都能差 60%，`§8-53`） | `docs/rounds/07-sokol-acceptance.md`、`docs/BACKEND.md` §13.10 |
| §37 | 10-06 | `svc_play_sfx2` 接入 | `0x25B45` 与 `0x25A96` **175 字节只差 17 字节**（6 个 rel32 + 5 处句柄立即数）⇒ 合共用体接入，接入 49→**50**；`typecheck` 1176→**1616 例全过**；顺带纠正 `sub_15F84` 的 ABI 测绘（**9 个栈参数**，不是 14 寄存器参数） | `docs/rounds/08-svc-sfx2.md`、`re/RE_MAP.md` |
| §38 | 10-06 | 起播闸门返工（音乐哑了） | §35 的闸门**把音乐整个堵死**（`stream_thread` 等 `WHDR_DONE`，而 `PrepareHeader` 只置 `0x2` ⇒ 4 个缓冲一个都没进 waveOut；音效另一条路所以照常）；就绪判据改成“没进过队列的就是我们的”，**判据升级到设备层**（`stream alive … pos/peak` 每 10 s 一行） | `docs/rounds/06-audio-fade.md` §35.7、`docs/PITFALLS.md` §8-54 |
| §38 | 10-06 | 起播闸门返工（音乐哑了） | §35 的闸门**把音乐整个堵死**（`stream_thread` 等 `WHDR_DONE`，而 `PrepareHeader` 只置 `0x2` ⇒ 4 个缓冲一个都没进 waveOut；音效另一条路所以照常）；就绪判据改成“没进过队列的就是我们的”，**判据升级到设备层**（`stream alive … pos/peak` 每 10 s 一行） | `docs/rounds/06-audio-fade.md` §35.7、`docs/PITFALLS.md` §8-54 |
| §39 | 10-06 | **脚本 VM 转译（主线）** | `0x15F84` 词流解释器 → `game/vm.c`：`case -1` 是与 `sub_15055` 共享的尾声 ⇒ `return cur`；12 个被调函数全部桩化对拍，**5512 例 0 失败**（当场抓到 `mode` 未写回、`dword_53C67` 无条件清零两个真 bug）；接入 50→**51**，A/B 三组 **0 px**，回归 8/8 | `docs/rounds/09-vm.md`、`docs/TRANSLATION.md` §4/§5 |
| §40 | 10-06 | 打字进行中配方 | 进一步证明 `vm_run`+`dlg_type_step` 在宿主里真跑过：`--shot-tick=326..334` 抓到**逐字画面**，框区差异 455→327→325→0，**15 字符 ↔ 15 tick ↔ `svc_wait_ticks(1)` 55 ms/字符**自洽；顺带修快流程（抓完即退 60 s→**22 s**）与 BMP→PNG 错误写法 | `docs/rounds/10-typewriter-recipe.md`、`docs/PITFALLS.md` §8-55 |
| §41 | 10-06 | **音频治本：软件混音器** | 音乐+音效收进 `audio.h` + `audio_sokol.c`（WASAPI，**设备每进程只开 1 次**）；增益仍在上游烘焙 ⇒ §11.6~§11.9 音量语义逐位不变；**新增 `--audio-dump` 可测判据**：逐秒 RMS 连续、`--volume` 10→100 实测 **10.3×**、`play 16`/`cut 0`、回归 8/8、A/B 0 px | `docs/rounds/11-audio-mixer.md`、`docs/AUDIO.md` §11.10、`docs/PITFALLS.md` §8-56 |
| §42 | 10-06 | **按键录制/回放**（用户需求） | `--keylog` 把“启动后第几毫秒按了什么”逐条落盘（崩溃也留）、`--keyplay` 按绝对时间重跑；独立成 `src/keylog.c`（**非宿主逻辑不进 host.c**，只留 5 个调用点）；判据：**录制↔回放同 tick 抓帧 0 px**、回归 8/8；顺带查清启动抢焦点混入杂键（`§8-57`） | `docs/rounds/12-keylog.md`、`docs/DEBUG-MANUAL.md`、`docs/PITFALLS.md` §8-57 |
| §44 | 10-07 | **补回 11 处漏掉的重定位**（Ghidra 桥复核） | §43 的"已解释差异"被第三家推翻：`le.c` 跳过的 22 条跨页 fixup 实为**两类** —— 11 条合法跨页（**必须写**，原本留下 11 个未重定位指针）+ 11 条 `src>0xFFF` 越界源（**必须跳**，§8-11 的崩溃出在这半）。修后 `applied=7948`、**三对象与 Ghidra 0 差异 exact match**、两平台一致、回归 8/8、同 tick A/B 0 px | `docs/rounds/14-fixup-boundary.md`、`docs/PITFALLS.md` §8-60 |
| §43 | 10-06 | **跨平台第 1 刀：`platform.h` + 加载器过河** | 抽出 OS 适配层（内存），`le.c` **零 Win32 依赖**；**`letest` 在 Windows 与 Linux 上三个对象 FNV-1a 哈希完全相同**（`fixups=7937` 同、`entry=0x3CCB4` 同）⇒ 加载器逐字节跨平台；新增 `platprobe`/`Makefile.linux`；踩到 `PROT_EXEC` 单bit坑（`§8-58`）与 `le.c` 里的 MSVC 内联汇编（`§8-59`） | `docs/rounds/13-portability.md`、`docs/PITFALLS.md` §8-58/§8-59 |
| §45 | 10-07 | **跨平台第 2 刀：`dos.c` 过河（故障分发 + 文件服务）** | 新增 `dos_fault.h` 契约：`dos_fault_core`（可移植，两平台同一套分支）+ `dos_fault_win.c`（VEH）/`dos_fault_posix.c`（sigaction+sigaltstack）；`dos.h` 去 `windows.h`、引入便携 `dos_ctx` 与自检入口 `dos_service`；platform.h 第 2 切片（文件/时间/线程/进程，`pread/pwrite` 调用方持位置）；**先测后写**：`faultprobe32`（freestanding -m32，不需要 multilib）实测 i386 compat 故障模型 = `SIGSEGV/SI_KERNEL` 三重身份且无 `si_addr`（`§8-61`）；新工具 **`doscheck`** 两平台**同套 49 条断言全过**；抓到并修掉 `MEM_RELEASE` 静默失败（`§8-62`）与 64 位上下文回写截断（`§8-63`）；回归 **8/8**、跨版本同 tick A/B **0 px** | `docs/rounds/15-dos-and-faults.md`、`docs/PITFALLS.md` §8-61..63 |
| §46 | 10-07 | **跨平台第 3 刀（起头）：入口层 —— 便携键表 + X11/XWayland 判定** | 用户问“Wayland 支不支持”：pin 住的 sokol **只有 X11 后端**（`_SAPP_LINUX` 全文件 0 处 wayland），但 X11 客户端在 Wayland 桌面经 **XWayland** 照跑；本机 WSLg 探针实测 `XWAYLAND=yes` + GLX 1.4 + RGBA visual（`docs/BACKEND.md` §13.11）。且 sokol X11 后端已内含 `XLookupString`（`SAPP_EVENTTYPE_CHAR`）与布局无关键码（`SAPP_KEYCODE`）⇒ 原计划“自己写 XLookupString/keysym 表”**收敛**为“`SAPP_KEYCODE`→便携键 id→BIOS 扫描码 + CHAR→ascii”。第一切片：`src/keys.h`+`keys.c`（`FR_KEY_LIST` 单真源）+`keys_win32.c`（VK 桥）+`keyscheck`（**102 键对拍 `MapVirtualKeyA` PASS**、5 个共享 make code alias）；当场抓到两个真 bug（`§8-64`） | `docs/rounds/16-entry-layer.md`、`docs/BACKEND.md` §13.11、`docs/PITFALLS.md` §8-64 |

## 4. 下一步计划（按优先级）

1. ~~**源码化继续**：`sub_15F84` 脚本 VM~~ **已完成（§39，2026-10-06）**：`game/vm.c` + `vmcheck`
   **5512 例全过**，接入分组 `vm`（接入 50→**51**）。**ABI 结论：9 个 cdecl 栈参数**（原记
   “14 寄存器参数”是 `0x3702F` 栈探针的伪影，`re/RE_MAP.md` 已更正）。
   下一个源码化目标看 `docs/TRANSLATION.md` §5；快照/还原 `0x15E9E`/`0x15E71` 仍与 CRT 堆
   整体替换一起接。
2. ~~**补 autokey 配方**~~ **已完成（§40，2026-10-06）**：标准配方 + `--shot-tick=326..334`
   即可落在“打字进行中”，抓到 3 张不同进度的逐字画面（框区差异 455→327→325→0），
   且 **15 字符 ↔ 15 tick ↔ `svc_wait_ticks(1)` 55 ms/字符**自洽 ⇒ `vm_run`+`dlg_type_step`
   确在宿主执行。抓图配 `--exit-when-file` 即时退出（**60 s → 22 s**），过渡段要重试（`§8-55`）。
3. ~~显示层换 sokol~~ **已验收（§36，2026-10-06）**：同 tick **基线 0 px**、GDI vs sokol **31 px（0.0484%）**，
   差异全在一块 14×4 的动画元素相位上；取样点定为 **`--shot-tick=600` 静止画面**（片头转场同后端
   自比都能差 60% ⇒ 先验基线再比跨后端，`docs/BACKEND.md` §13.10、`docs/PITFALLS.md` §8-53）。
   `--render=gdi` 不进日常循环，只在需要参考实现时按需重建。
4. ~~**音频治本**~~ **已完成（§41，2026-10-06）**：音乐与音效收进 `src/audio.h` +
   `src/audio_sokol.c`（sokol_audio/WASAPI，**一个设备、一把递归锁、一个回调里相加**），
   `synth.c` 不再有流线程/缓冲队列（回调按需拉 ⇒ 音量零延迟），`ail.c` 不再有每句柄设备。
   **增益仍在上游烘焙**，§11.6~§11.9 音量语义逐位不变；`--volume` 实测 10.3×。
   新增 **`--audio-dump=<wav>`** 让音频判据可测量（逐秒 RMS、分声道峰值）。详见 `docs/AUDIO.md` §11.10。
5. **稳定性长跑**：连续 5 分钟以上与反复重启（退出路径已验）。
6. **存档路径实测**：`FD2.SAV` 从无到有的创建路径、存档变小后的截断对拍；
   `AH=49/4A` 仍是空操作（账本只增不减）。
7. **跨平台**：单代码库 + 后端选择（**不用 git 分支**），先 Linux x86-64；顺序见 `docs/BACKEND.md`。
   **第 1 刀已落（§43）**：`platform.h` 内存层 + `le.c` 零 Win32 依赖，`letest` 两平台哈希完全一致。
   **第 2 刀已落（§45）**：`dos.c` 过河 —— `dos_fault.h` 故障契约（`dos_fault_core` 可移植 +
   VEH/sigaction 两个薄包装）、`dos_ctx` 便携寄存器帧、platform.h 第 2 切片（文件 `pread/pwrite`
   调用方持位置、线程/时间/进程）、低内存镜像本就走 `plat_*`；**Linux 故障模型先实测**
   （`faultprobe32`：`SIGSEGV/SI_KERNEL` 三重身份、无 `si_addr`、int3 的 EIP 已越过，`§8-61`）；
   新工具 **`doscheck`**（两平台**同一套 49 条断言**，含真 `int 0x21` 经 VEH/sigaction 分发、
   CF 回写）两平台全过；回归 8/8、跨版本同 tick A/B 0 px。
   **第 3 刀（入口层）已起头（§46）**：先回答“Wayland 能不能用” —— sokol 只有 X11 后端，
   但 X11 客户端经 **XWayland** 照跑（本机 WSLg 实测 `XWAYLAND=yes`）；sokol 已内含
   `XLookupString`/布局无关键码 ⇒ **不用自己写 X11 代码**。第一切片 = 便携键表
   `src/keys.c` + `keys_win32.c` + **`keyscheck`（102 键对拍 `MapVirtualKeyA` PASS）**，
   解决了 `MapVirtualKeyA/ToAscii` 的跨平台替代（`docs/BACKEND.md` §13.11）。
   **剩下的入口层活**：把它接进 `host.c`/`keylog.c`/两个入口层（`input_post_key`）、
   `platform.h` 第 3 切片（时间/线程/目录/文件属性/log 重定向）、Linux `main_sokol.c` 去 Win32，
   最后 **`-m32`** 跑真游戏（游戏是 32 位 x86，64 位进程跑不了 ⇒ `gcc-multilib` + 32 位
   X11/ALSA，apt candidate 已确认；届时用 `faultprobe32` 复核故障模型表）。`--screenshot`
   本来就在共享层直接写 BMP，Linux 侧不受影响（`--wshot` 先 stub）。见
   `docs/rounds/16-entry-layer.md`、`docs/rounds/13-portability.md` §43.5、`docs/rounds/15-dos-and-faults.md` §45.7。
8. ~~FDPS（炎龙外传）~~ **已冻结**（2026-10-05 用户决定）：成果与卡点存档在 `docs/FDPS-ARCHIVE.md`，
   宿主的通用能力（`--exe`、FDPS AIL 表、定时器线程、INT9 注入）留在代码里不再主动维护。

## 5. 冻结 / 不再投入

| 项 | 状态 | 存档 |
|---|---|---|
| FDPS（炎龙外传） | 冻结（2026-10-05 用户决定） | `docs/FDPS-ARCHIVE.md` |
| SDL2 / SDL3 | 降为备选记录 | `docs/BACKEND.md` §13.2 / §13.3 |
| ~~鼠标 `INT 33h`~~ | 已判定不需要 | `docs/rounds/01-platform-and-tooling.md` §12.3 |
