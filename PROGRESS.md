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

**同一份源码也能在 Linux 上跑**（`build/fd2host-linux32`，X11/XWayland + sokol GLCORE）：
与 Windows 同 guest tick 抓帧**逐像素 0 差异**（`rounds/16-entry-layer.md` §46.10）。

**当前重心是路线 C 的主体：逐步源码化。** 已把 65 个函数从机器码还原成 C、经 `src/repl.c`
接入运行中的游戏（逐字节对拍 + `regress.ps1` 8/8）。

> ⚠ **进度必须看清**：全量函数表 `re/funcmap.csv` 有 **1359** 个函数，已源码化的只有
> **65 个 ≈ 4.8%**；**其余 ~96% 仍然是 `FD2.EXE` 里的原始 32 位 x86 机器码，由宿主在本进程里
> 直接执行**。这正是宿主必须是 **32 位进程**的原因（x86-64 长模式不能执行 32 位代码，
> 只有 `-m32`/WOW64 这类 32 位进程才行）；**等全部函数源码化后，这个 32 位门槛才会消失**。

方法总览见 **`docs/TRANSLATION.md`**，下一个转译目标看它的 §5。

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
源码转译：**65 / 1365 个函数接入（≈4.8%）**           ⏳ 其余 ~96% 仍是原始机器码在跑
Linux 原生：`fd2host-linux32` 跑真游戏，与 Windows 同 tick 抓帧 **0 px** ✅（§46.10）
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
| §46 | 10-07 | **跨平台第 3 刀（起头）：入口层 —— 便携键表 + X11/XWayland 判定** | 用户问“Wayland 支不支持”：pin 住的 sokol **只有 X11 后端**（`_SAPP_LINUX` 全文件 0 处 wayland），但 X11 客户端在 Wayland 桌面经 **XWayland** 照跑；本机 WSLg 探针实测 `XWAYLAND=yes` + GLX 1.4 + RGBA visual（`docs/BACKEND.md` §13.11）。且 sokol X11 后端已内含 `XLookupString`（`SAPP_EVENTTYPE_CHAR`）与布局无关键码（`SAPP_KEYCODE`）⇒ 原计划“自己写 XLookupString/keysym 表”**收敛**为“`SAPP_KEYCODE`→便携键 id→BIOS 扫描码 + CHAR→ascii”。第一切片：`src/keys.h`+`keys.c`（`FR_KEY_LIST` 单真源）+`keys_win32.c`（VK 桥）+`keyscheck`（**102 键对拍 `MapVirtualKeyA` PASS**、5 个共享 make code alias）；当场抓到两个真 bug（`§8-64`）。**第二切片（接线）**：`input_post_vk` → `input_post_key(fr_key)`，`host.c` 的 `vk_from_name` 删掉、`keylog.c` 名称/回放全部改成便携键（旧 `#<vk>` 仍可回放），**regress 8/8 + 录制↔回放同 tick 0 px**；注释里 `*/` 踩坑记 `§8-65`。**第三切片（平台层第 3 切片 + host.c/keylog.c 过河）**：`platform.h` 加 `plat_now_ms/plat_thread_stk/plat_set_cwd/plat_module_path/plat_path_size/plat_stricmp/plat_strdup/plat_stdio_pin`，`host.c`/`keylog.c` **零 `windows.h`**（BMP 头手写小端、`__cdecl` → `FD2_CDECL`）；Windows regress **8/8** + 回放 **0 px**，Linux `letest` 三哈希 exact match + `doscheck` **49/49**。**第四切片（Linux 宿主）**：`main_sokol.c` 分平台（POSIX 用 `SAPP_KEYCODE→fr_key` + `SAPP_EVENTTYPE_CHAR`），音频/AIL 栈过河（`plat_mutex/plat_atomic_*/plat_now_us`，去掉 `windows.h`），新增 `make -f Makefile.linux build/fd2host-linux`（64 位**链接证明**，0 warning）；**真能跑的 `host32`（-m32）被 i386 工具链阻塞（本机 sudo 要密码）**。顺带修 `game/svc.c` 的 `__cdecl`/`uintptr_t`。**重申：51/1359 ≈ 3.8% 已源码化，其余 ~96% 仍是机器码且 C 函数会回调其固定 32 位地址 ⇒ 必须 32 位**（新写的 `rounds/16` §46.8） | `docs/rounds/16-entry-layer.md`、`docs/BACKEND.md` §13.11、`docs/PITFALLS.md` §8-64/§8-65 |
| §47 | 10-07 | **第 52 个转译函数：`res.c` 接入 + `guest_mem` 堆缝** | `res.c`（`0x111BA`）对拍 160 例早过，卡的是**跨 C/机器码边界的堆**：它 `free(old_buffer)`、返回的 `buf` 又被游戏释放。IDA+现有代码结论：游戏堆入口是 `0x3706E malloc`/`0x3776E free`，**不能只换 libc**（CRT 内部 stdio 也用自家堆）⇒ 抽**唯一堆缝** `src/game/guest_mem.h`（宿主=游戏堆，check=已重定向的宿主 libc，将来 64 位=宿主 malloc）＋`guest_store_u32` 写游戏全局；`res.c` 三处 `malloc/free` 改 `guest_malloc/free`、尺寸写 `0x53BFF`；`rescheck` 改快照 `GUEST_SIZE`；`repl` 新分组 `res`。判据：`rescheck` **160/0**、`regress` **8/8**、`repl: installed 52`、`none↔all` 同 tick **0 px**、Linux `host32` 同 tick **0 px** | `docs/rounds/17-res-and-guest-heap.md`、`docs/TRANSLATION.md` §4/§5、`docs/PITFALLS.md` §8-68 |
| §48 | 10-07 | **快照对 `0x15E9E`/`0x15E71` 转译（第 53/54 个）** | 反汇编结论：`0x15E71` = `gfx_restore_rect(rec,surface,stride)+free(rec)`；`0x15E9E` = `malloc(w*h+8)` + `gfx_save_rect` + `gfx_blit_transparent`。新增 `dlg_snap_save/dlg_snap_restore`（走 §47 的 `guest_mem`），`dlg.c` 不再调 `ORIG_SNAP_*`；**patch `0x15E71` 安全**：未转译调用者的记录也来自同一 Watcom 堆。判据：`boxcheck` **240/0**、`dlgcheck/typecheck/keycheck` **800/1616/100 全 0**、`regress` **8/8**、`repl: installed 54`、`none↔all` 同 tick **0 px**、Linux `host32` 同 tick **0 px** | `docs/rounds/18-snapshot-pair.md`、`docs/TRANSLATION.md` §4/§5 |
| §49 | 10-07 | **主状态机族第一刀：`play_bgm`（`0x25977`）转译（第 55 个）** | IDA 侦察：`0x25977` 不是状态机本体，而是**唯一换曲入口**（32 调用点），只调服务 ⇒ 适合开刀；顺带确认 `0x3666C` = DPMI `lock linear region`（平坦宿主 no-op），并更正 `re/RE_MAP.md`。`game/bgm.c` 的服务全走原地址（res_load + 5 个 AIL 序列入口）；新增 `bgmcheck`：**当场抓到真 bug**——`movzx byte_51A11; cmp eax,arg_0` 不能写成 `(uint8_t)` 比较（`track==-1` 时 `0xFF!=0xFFFFFFFF`，`§8-69`）。判据：`bgmcheck` **6000/0**、`regress` **8/8**、`repl: installed 55`、`ail: …ramped` 仍在、`none↔all` 同 tick **0 px**、Linux `host32` 同 tick **0 px** | `docs/rounds/19-play-bgm.md`、`docs/PITFALLS.md` §8-69、`re/RE_MAP.md` |
| §50 | 10-07 | **主状态机族第二刀：`scene_card`（`0x22E5C`）转译（第 56 个）** | 顺着 `main`(`0x25BF4`) 调用图取最小场景（154 B、纯服务序列）：`bgm_play(-1,1)`/`svc_wait_ticks`/调色板淡出淡入/res_load(FDOTHER,79)/memset VGA/RLE 子图×2/free；同轮**钉死两张分派表** `funcs_25E23 @0x51DE9`、`funcs_25E3A @0x51D71`（更正 RE_MAP）。`scenecheck` 把 7 个服务 hook 成记录桩比对调用序列，**100/0**；`regress 8/8`、`repl: installed 56`、`none↔all` 同 tick **0 px**、Linux `host32` **0 px**。**记一次偶发**：首跑 regress 未达退出条件（跑到 60 s 上限）而失败，重跑 8/8，暂标待确认（疑启动竞争，`rounds/20` §50.4） | `docs/rounds/20-scene-card.md`、`docs/TRANSLATION.md` §4/§5、`re/RE_MAP.md` |
| §51 | 10-07 | **调色板淡变三件套转译（第 57–59 个）** | `0x11D40` 区间淡变（`outp 0x3C8/9`，`pal-*(0x53A65)` 下限 0）+ `0x1F882` 变暗（sub 0→63）+ `0x1F525` 变亮（64→0），共 29 个调用点；`game/fade.c`，服务仍走原地址（`outp`/`delay`）。`fadecheck` **4000/0**；顺带修掉对拍自身越界（1024 事件/步 × 65 步 ≈ 66k，改用 FNV 哈希+计数，`rounds/21` §51.2）。判据：`regress` 8/8、`repl: installed 59`、`none↔all` 同 tick **0 px**、Linux 只做构建+`letest`/`doscheck`（新分级） | `docs/rounds/21-fade.md`、`docs/TRANSLATION.md` §4 |
| §52 | 10-07 | **热叶子六件套（第 60–65 个）** | 按 `re/func_ranking.csv` 的用量排序取 6 个叶子：`kbd_flush`(64)/`kbd_pending`(17)（BDA，走 `DOS_LOWMEM_BASE` 镜像）、`util_rand`(40)、`gfx_copy_rows`(115)、`res_blit`(82，调已对拍 `rle_decode`)、`dlg_portrait_glide`(28)，共约 350 调用点；新 `kbd.c` + `src/leafcheck.c`（72008 例，含真实 FDOTHER.DAT 子图 × 4 mode）。踩坑：`rle_decode` 目的地偏移用实参 x/y、尺寸取流头（`§8-70`）。判据：`leafcheck` **72008/0**、`regress` 8/8、`repl: installed 65`、A/B **0 px**、Linux 构建+自检（新分级） | `docs/rounds/22-hot-leaves.md`、`docs/PITFALLS.md` §8-70 |

## 4. 下一步计划（按优先级）

1. **源码化继续**（工作重心）。~~`sub_15F84` 脚本 VM~~ **已完成（§39）**；
   ~~`res.c` 接入~~ **已完成（§47）**（`src/game/guest_mem.h/.c` 唯一堆缝）；
   ~~`0x15E9E`/`0x15E71` 快照对~~ **已完成（§48）**（`dlg_snap_save/restore`，接入 **54**）。
   **下一个目标**（`docs/TRANSLATION.md` §5）：**主状态机**
   `0x25977`/`0x25EBB`/`0x117E7`/`0x22E5C`/`0x26152`（含 `funcs_25E23[]`/`funcs_25E3A[]`
   函数指针表）——最大一块，一条一条转 + `*check` 对拍；
   记录维持：`repl.c` 加行 → `python tools/translation_map.py`（现 65/1359）。
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
   `XLookupString`/布局无关键码 ⇒ **不用自己写 X11 代码**；**用户拍板保持 X11/XWayland**
   （原生 Wayland 的自写 Vulkan 方案成本评估见 `BACKEND` §13.11）。已完成：便携键表
   `src/keys.c` + `keys_win32.c` + **`keyscheck`（102 键对拍 `MapVirtualKeyA` PASS）**，
   并接进 `host.c`/`keylog.c`/两个入口层（`input_post_key`）—— **regress 8/8 + 录制↔回放同 tick 0 px**。
   **平台层第 3 切片已落**：`host.c`/`keylog.c` **零 `windows.h`**（`plat_now_ms/thread_stk/set_cwd/
   module_path/path_size/stricmp/strdup/stdio_pin`），Windows regress 8/8 + 回放 0 px，
   Linux `letest` exact match + `doscheck` 49/49。
   **第 4 切片（Linux 宿主）已落**：`main_sokol.c` 分平台（POSIX 用 `SAPP_KEYCODE→fr_key` +
   `SAPP_EVENTTYPE_CHAR`，`host_key_set_last_ascii` 回填 ascii）；音频/AIL 栈过河
   （`plat_mutex`/`plat_atomic_*`/`plat_now_us`，`audio_sokol/ail/synth/xmidi/dls/repl` 零 `windows.h`）；
   `Makefile.linux` 新增 `build/fd2host-linux`（64 位**链接通过**）与 `host32`（-m32，真能跑）。
   **已跑通（§46.10）**：用户装好 i386 工具链后 `host32` 在 **WSLg/XWayland** 上跑真游戏
   （GLCORE，7948 fixup、65 低内存引用、52 AIL 打桩），与 Windows 同 tick `--screenshot`
   **0 / 64000 px** 差异。
   **关于 32 位**：用户要求终局**不再用 32 位**（要能上 macOS，而 macOS 已无 32 位）。
   现状 65/1359 ≈ 4.8% 已源码化、其余仍是机器码且 C 会回调其固定 32 位地址 ⇒ 过渡期必须 32 位；
   路线（A 转译 → B 转完 → **C 去 guest 化** → D 64 位/三平台）见 `docs/TRANSLATION.md` §6、
   `rounds/16-entry-layer.md` §46.11。**下一步（工作重心）：按 §5 继续源码化**（51 → 1359）。
   同轮补：**Linux 窗口 180° 倒置修好**（GLSL 多翻一次 v；`--screenshot` 看不到窗口翻转，
   新增 `FD2_TESTPATTERN`/`FD2_GL_READBACK` 定向自检：修复前 `flipped=0`、修复后 `upright=0`，
   `PITFALLS` §8-67）；**转译记录/map** = `re/translation_map.csv` +`tools/translation_map.py`
   （`--check` 防漂移；65 wired / 1359）。
8. ~~FDPS（炎龙外传）~~ **已冻结**（2026-10-05 用户决定）：成果与卡点存档在 `docs/FDPS-ARCHIVE.md`，
   宿主的通用能力（`--exe`、FDPS AIL 表、定时器线程、INT9 注入）留在代码里不再主动维护。

## 5. 冻结 / 不再投入

| 项 | 状态 | 存档 |
|---|---|---|
| FDPS（炎龙外传） | 冻结（2026-10-05 用户决定） | `docs/FDPS-ARCHIVE.md` |
| SDL2 / SDL3 | 降为备选记录 | `docs/BACKEND.md` §13.2 / §13.3 |
| ~~鼠标 `INT 33h`~~ | 已判定不需要 | `docs/rounds/01-platform-and-tooling.md` §12.3 |
