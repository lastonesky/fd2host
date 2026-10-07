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

**当前重心是路线 C 的主体：逐步源码化。** 已把 133 个函数从机器码还原成 C、经 `src/repl.c`
接入运行中的游戏（逐字节对拍 + `regress.ps1` 8/8）。

> ⚠ **进度必须看清**：全量函数表 `re/funcmap.csv` 有 **1359** 个函数，已源码化的只有
> **133 个 ≈ 9.8%**；**其余 ~90.2% 仍然是 `FD2.EXE` 里的原始 32 位 x86 机器码，由宿主在本进程里
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
源码转译：**133 / 1359 个函数接入（≈9.8%）**           ⏳ 其余 ~90.2% 仍是原始机器码在跑
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
| §53 | 10-07 | **地图视图叶子四件套（第 66–69 个）** | 按 ranking 取依赖闭合的 4 个：`map_blit_tile`(0x126F7)、`pal_fade_add`(0x11DF2，加+上限 0x3F)、`res_blit6`(0x16886，偏移表在 +6)、`dlg_portrait_clear`(0x134E4)；调用图里没有未转译游戏函数。新 `game/map.c` + `src/mapcheck.c`（**25000/0**），`fadecheck` 扩到 4 模式；`regress 8/8`、`repl: installed 69`、A/B **0 px**、Linux 构建+自检 | `docs/rounds/23-map-view.md` |
| §54 | 10-07 | **动画/地图格/头像查找四件套（第 70–73 个）** | 沿 `0x11CAC` 的依赖拓扑收全闭合的 4 个：`anim_frame_step`(0x1297D，BDA tick 帧计数)、`map_cell_info`(0x12E38)、`dlg_portrait_find`(0x12C0D)、`tbl_off627D8`(0x4EB48)；新 `game/anim.c`；`mapcheck` 加低内存重定向后 **55500/0**。`regress 8/8`、`repl: installed 73`、A/B **0 px**、Linux 构建+自检 | `docs/rounds/24-anim-cell.md` |
| §55 | 10-07 | **数字渲染链（第 74–76 个）** | `dlg_draw_number`(0x187D6，`%0.Nd` + 逐位 sprite)、`dlg_draw_number_pair`(0x1875D，按相等选 0x1F/0x2A)、`dlg_draw_number_signed`(0x1AEB1，符号 0x83/0x84 + `abs`)；`mapcheck` 合成 256 子图后 **75500/0**；顺带把链 `dlg.c` 的 5 个 harness 补上 `res.c`/`rle.c`/`rec.c`（链接期才炸的漏项）。`regress 8/8`、`repl: installed 76`、A/B **0 px**、Linux 自检全过 | `docs/rounds/25-number-render.md` |
| §56 | 10-07 | **格子对象精灵链（第 77–79 个）** | 闭 `0x127A9` 依赖：`rec_skip`(0x1F183)、`map_blit_cell_sprite`(0x12AC6，`*(0x53A5D)` 精灵库偏移表 +0x0A / `*(0x53A6D)` 调色板库 +6、plain 与 pal 两路)、`map_refresh_records`(0x129EC)；`mapcheck` 合成三张库表后 **86500/0**（其中 `map_refresh_records` 同时验证 C 版 0x12AC6）。`regress 8/8`、`repl: installed 79`、A/B **0 px**、Linux 自检全过 | `docs/rounds/26-cell-sprites.md` |
| §57 | 10-07 | **头像精灵链（第 80–81 个）** | `0x127E0` 画单条记录的 24×24 头像/图标（`*(0x53A61)` 32 位偏移表，索引 `mode+12*p[2]+3*p[3]`；每 BIOS tick 翻 `dword_53A04`；`p[5]` bit7 走 `sprite24_ramp24` 否则 `sprite24_plain`）+ `0x127A9` 扫全部未标记记录后 `map_refresh_records`；`mapcheck` 合成 48 帧头像库后 **97000/0**。踩坑：harness 给 `dword_53A04` 随机 int32 把原机器码写出位图 → 段错误（§8-71）。`regress 8/8`、`repl: installed 81`、静态帧 `--shot-tick=500` A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/27-portrait-draw.md`、`docs/PITFALLS.md` §8-71 |
| §58 | 10-07 | **角色记录 8 槽字段访问器簇（第 82–85 个）** | `rec.c` 加 4 个纯数据叶子：`rec_field_byte`(0x1B722，槽值字节 +11+2·slot)、`rec_status_set`(0x344F2，闭区间 +52 低半字节 OR，`value` 高半字节**不截断**、`jle` 有符号)、`rec_slot_claim`(0x1BB8C，占首空槽 bit7→01)、`rec_slot_remove`(0x1B8E7，memmove 左移删除 + 末槽 0x80)；`reccheck` 写函数用双副本逐字节比对，**28739→32010/0**；`mapcheck`/`utilcheck` 仍 97000/2200。踩坑：残留 `fd2host.exe` 锁住构建产物（§8-72）。`regress 8/8`、`repl: installed 85`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/28-rec-slots.md`、`docs/PITFALLS.md` §8-72 |
| §59 | 10-07 | **持久队伍记录表三件套（第 86–88 个）** | 新建 `game/unit.c`：`unit_recalc`(0x1145A，8 槽物品加成累加到 +48..+4E，32 位累加/16 位写回、返回未截断 +4E)、`unit_refresh_all`(0x11506，外角色×内队伍 `+8` 身份匹配后整笔抄回 + 清 transient + `+5&=1` + 同步 + recalc)、`unit_add`(0x112A5，`tbl_61DA1` 默认 + `tbl_620A1` 成长构造记录 append，`(L-1)` 32 位成长、`+17/+19` 保留残值)；`reccheck` 双副本逐字节 **32010→36327/0**，`mapcheck`/`utilcheck` 97000/2200。转完闭合 `funcs_25E23` 分派表 5 个表项的依赖闭包。`regress 8/8`、`repl: installed 88`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/29-unit-roster.md`、`re/RE_MAP.md` |
| §60 | 10-07 | **主状态机转移 handler 五件套（第 89–93 个）** | 扩 `game/scene.c`：`funcs_25E23`(`0x51DE9`) 转移表 5 个表项 `0x22EF6`[0]（`vm_run(…,9,…)`+refresh 后 `dword_53C03=1`、**赋值非自增**）/`0x231BC`[3]（sub=4）/`0x23790`[10]（sub=3 + `unit_add(14)`）/`0x2389F`[12]（sub=9 + `unit_add(3)`）/`0x23E39`[18]（**refresh 在前**、sub=3），每个 59–69 B、纯服务序列（零 VGA/堆/IO，依赖 3 个已接入服务 + CRT 栈探针）；`scenecheck` 新增 vm/unit 三桩 + 1 KiB 全局快照，**100→1100/0**。踩坑：手搓 `--exit-after=30` 抓不到 `--shot-tick=500`（18.2 Hz ⇒ 需 ~31 s，§8-73）。`regress 8/8`、`repl: installed 93`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/30-scene-states.md`、`docs/PITFALLS.md` §8-73、`re/RE_MAP.md` |
| §61 | 10-07 | **`funcs_25E23[14]` 状态 handler + 队伍身份查询叶子（第 94–95 个）** | `game/scene.c` 加 `scene_state_14`(0x239BD)：`unit_exists(12)` 决定 `sub=(al^1)+12`（存在=12/否则=13）→ `vm_run` → `unit_refresh_all` → `unit_add(15)` → `dword_53C03++`；`game/unit.c` 加 `unit_exists`(0x33499)：扫 `dword_53BF7` 起 `dword_53BFB` 条 80 B 记录，`movzx byte[+8]` 与**完整 32 位 id** 比较（有符号计数、命中首条返回 1，被 7 处调用）。闭包只有这一个 64 B 叶子（`vm_run`/`unit_refresh_all`/`unit_add` 均已接入）⇒ 闭合 `funcs_25E23` 第 6 个表项。`scenecheck` **1100→1480/0**（多驱动 `unit_exists` 返回值 0/1/0x100/0x101/0xFF/-1）、`reccheck` **36327→37398/0**。`regress 8/8`、`repl: installed 95`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/31-scene-state-14.md`、`re/RE_MAP.md` |
| §62 | 10-07 | **`funcs_30469` 效果动画 handler 四件套（第 96–99 个）** | 新模块 `game/fx.c/.h`：`funcs_30469`(`0x524C6`) 表项 `[4]/[7]/[8]/[9]` = `fx_dots6`(0x2C217)/`fx_dots3`(0x2CAFC)/`fx_dots16`(0x2CCF4)/`fx_toggle`(0x2CE1A)，6/3/16 粒子发射器 + 双帧翻转，共 1630 B、60 到达点；只读写游戏数据段全局 + 原址 `memcpy` 三张只读表 + 调 4 个已接入服务（`res_blit`/`svc_play_sfx(2)`/`util_rand`），无文件/堆/VGA。**真 ABI = cdecl 5 栈参**（探针伪像，见 `rounds/08` §37.3 / §8-75）；`fxcheck` 新增记录桩 + 全区 obj1 快照，**3920/0**；接入新分组 `REPL_FX`。踩坑：Hex-Rays 9 参视图把 `fx_toggle` case 4 的 index 从 `0` 伪装成 `4`（§8-75）。`regress 8/8`、`repl: installed 99`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/32-fx-handlers.md`、`docs/PITFALLS.md` §8-75、`re/RE_MAP.md` |
| §63 | 10-07 | **`funcs_30469` 收尾：剩余 5 handler + 1 辅助（第 100–104 个）** | `game/fx.c` 扩 `fx_dots7`(0x2B996)/`fx_dots8`(0x2BB33)/`fx_blob`(0x2BD6C)+`fx_advance`(0x2BF83)/`fx_dots12`(0x2BFD9)/`fx_dots6b`(0x2C441)，共 **2749 B**、75 个到达点 + 2 个内部调用；表 `[0]/[1]/[2]/[3]/[5]` 完成 ⇒ **整表 9/10**（仅余 `[6] 0x2C67D` 1151 B 含 CRT 浮点）。依赖仍全闭合（只读表原址 `memcpy` + 4 个已接入服务 + CRT 栈探针）。`fxcheck` 扩到 **12884/0**（新增 8964，含 `fx_advance` 资源块直测）。**当场抓到 2 个真 bug**：`fx_dots8` blit 上界是 `<0xF` 非 `<0x10`；`fx_dots12` `++phase==3` 时 `r=1` **无条件**（sfx2 才看 `v20[slot]==0`）。`regress 8/8`、`repl: installed 104`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/33-fx-tail.md`、`docs/TRANSLATION.md` §4/§5、`re/RE_MAP.md` |

| §64 | 10-07 | **角色记录表七个数据叶子（第 105–111 个）** | 第 28 轮排好队的依赖闭合叶子：`rec_slot_free`(0x1B8A6，八槽 state bit7 清零个数)/`rec_slot_find`(0x1B83D，bit6 + 按 `want_high` 比 value `<0x80` 或 `>=0x80`，无则 -1)/`rec_sub_table5`(0x1CA89，**16 位字** `+68 -= 0x619FD` 表项字节 5，返回记录地址)/`rec_flag_or80`(0x13512)/`rec_flag_set1`(0x32975)/`rec_status_mask_records`(0x34D64，固定记录 10..27)/`rec_status_set_record14`(0x35009)，共 375 B、69 个到达点；**6 个返回值语义只有 1 个是记录地址**（`0x13512`/`0x32975` 返回 `80*index` 偏移，`0x34D64` 返回表基址，`0x35009` 返回 `base+1120`）。收 `funcs_1199C`(`0x51B91`) `[28]`/`[36]`；排除 `0x205BE`（真入口是 `0x205B4`，条目不合法）。`reccheck` **37398→42225/0**（+4827）。顺手修 2 个工具 bug：`repl_parse()` 漏认 `map` 组名（`--replace=map` 静默等同 `none`，§8-77）；`rec.c` 新引 `tbl_ptr` 使 5 个 check target **链接期**缺 `tables.c`（§8-78）。`regress 8/8`、`repl: installed 111`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/34-rec-leaves.md`、`docs/PITFALLS.md` §8-77/§8-78、`re/RE_MAP.md` |
| §65 | 10-07 | **对话框/头像合成三件套（第 112–114 个）** | 第 28 轮显式后置的 `0x1956B`(`msg_open_portrait`)/`0x1974C`(`msg_blit_band`)/`0x26996`(`msg_close_portrait`)，共 **624 B / 103 个直接调用点**；新模块 `game/msg.c/.h`，接入既有 `REPL_DLG`（原子开关，`--replace=dlg` → 21）。三个 64000 B 屏缓冲留在原地址全局 `dword_53C5B/F/63`（48/62/91 个未转译点共享），堆走 `guest_mem`——这正是当年后置的唯一理由。真 ABI=cdecl（栈探针伪像），`0x16F04` 只是共享尾声→`return`。`msgcheck` **465/0**（事件序列归一化指针 + 整幅 VGA + 三屏缓冲逐字节；`0x1974C` 故意不钩，open/close 两侧跑真条带；故障注入 2 次均当场抓到）。`regress 8/8`、`repl: installed 114`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/35-msg-portrait.md`、`docs/TRANSLATION.md` §4/§5、`re/RE_MAP.md` |
| §66 | 10-07 | **`funcs_1199C` 事件 handler 闭合子集（第 115–125 个）** | 第 35 轮点名的 `funcs_1199C`(`0x51B91`) 里**现在就能闭合的 11 个 handler**（`0x34738`/`0x348EA`/`0x34A6C`/`0x34B2F`/`0x34CF1`/`0x34D92`/`0x34F74`/`0x35123`/`0x35191`/`0x351E6`/`0x35258`，共 944 B / 99 个直接到达点 + 表项），纯整数事件序列（`status_set` + `vm_run` + `rec_flag` + 一次性标志），依赖全闭合（只调已接入的 `vm.c`/`rec.c`）；新模块 `game/ev.c/.h`，接入既有 `REPL_REC`（`--replace=rec` → 29）。**`_chkstk` 保持 EAX ⇒ 4 个函数的 EAX 是调用者垃圾值 / 服务返回值**，逐个由入口 `retn` 定案为 `void`。`evcheck` **2940/0**（6 服务记录桩 + 256×80 记录表逐字节 + 事件序列 + 归一化返回值；故障注入 3/3 抓到）。`regress 8/8`、`repl: installed 114 → 125`、静态帧 A/B **0/64000 px**、Linux 自检全过 | `docs/rounds/36-ev-handlers.md`、`docs/TRANSLATION.md` §4/§5、`docs/PITFALLS.md` §8-79、`re/RE_MAP.md` |
| §67 | 10-07 | **地图视图渲染核（第 126–129 个）** | 第 36 轮点名的“场景渲染核”里**依赖已全闭合的 4 个**：`map_render_view`(0x11EEE，按视图模式把 w×h 个 24×24 格画进 `dst`，含扫描线扩展/滚动/闪烁三特例 + 镜像/坡道标志)、`map_scroll_lines`(0x24D22，312×192 屏缓冲环绕滚动，堆走 `guest_mem`)、`map_reveal_cursor`(0x122DC，按半径画 1/5/13/21 格菱形揭示，case 6 清可见位)、`map_draw_cursor`(0x1ACF3，叠选择框/头像/数字/血条)，共 **2589 B / 37 个到达点**；并入既有 `game/map.c`（`REPL_MAP`）。**关键手法：先用 `apply_tinfo` 把真 cdecl 原型写回 IDA，再反编译**（否则 6 栈参看成 10 寄存器参、41 个 `map_blit_tile` 调用乱序；§8-80）。`mapcheck` **97000→112000/0**（新增 15000；CRT `malloc/memmove/free` 重定向 + 三块缓冲逐字节 + 6 相位全局；故障注入 4/4 抓到）。`regress 8/8`、`repl: installed 125 → 129`、静态帧 A/B **0/64000 px**、Linux 构建 0 warning + 自检全过 | `docs/rounds/37-map-view-core.md`、`docs/TRANSLATION.md` §4/§5、`docs/PITFALLS.md` §8-80、`re/RE_MAP.md` |
| §68 | 10-08 | **调色板动画 + 地图视图刷新（第 130–133 个）** | 沿 `0x11CAC` 的**唯一剩余依赖**收全：`pal_tick_word`(0x4E310，BDA tick 零扩展)、`pal_anim_step`(0x4E31C，每 ≥2 tick 把 `0x60003` 起 48 B 按帧 `lodsb` 上传 DAC `0xE0..0xEF`；**内联 `out dx,al`**)、`map_unit_ping`(0x32230，记录 `+32`→29 B 表→`%6/%4/%9` 音效)、`map_view_update`(0x11CAC，帧/tick 动画 + w×h=13×8 视图合成 + 推回 `0xA0504` VGA)，共 **496 B / 104 个到达点**；`fade.c`+`map.c`，接入 `REPL_FADE`/`REPL_MAP`。**唯一新工程件=窄 VEH `out` 陷阱**（只认 `0x4E31C` 范围内 `EE` 这一条，与 C 侧 `stub_outp` 汇入同一 DAC 事件日志）。`mapcheck` **112000→122500/0**（故障注入 4/4）；闭环价值：全表 usage 前二 `0x135DD`(98)/`0x1366A`(110) 依赖闭合。`regress 8/8`、`repl: installed 129 → 133`、静态帧 A/B **0/64000 px**、Linux 构建 0 warning + `letest` exact match + `doscheck 49/49`。新蹈坑：非关键 VGA 块未提交导致 harness 间歇 AV（§8-83）、域外 `t[k-1]` 不可复现（§8-84） | `docs/rounds/38-palette-and-map-refresh.md`、`docs/TRANSLATION.md` §4/§5、`docs/PITFALLS.md` §8-83/84、`re/RE_MAP.md` |

## 4. 下一步计划（按优先级）

1. **源码化继续**（工作重心）。~~`sub_15F84` 脚本 VM~~ **已完成（§39）**；
   ~~`res.c` 接入~~ **已完成（§47）**（`src/game/guest_mem.h/.c` 唯一堆缝）；
   ~~`0x15E9E`/`0x15E71` 快照对~~ **已完成（§48）**（`dlg_snap_save/restore`，接入 **54**）。
   **下一个目标**（`docs/TRANSLATION.md` §5）：**主状态机**
   `0x25977`/`0x25EBB`/`0x117E7`/`0x22E5C`/`0x26152`（含 `funcs_25E23[]`/`funcs_25E3A[]`
   函数指针表）——最大一块，一条一条转 + `*check` 对拍；
   ~~`0x127A9` 头像链~~ **已完成（§57，接入 81）**；
   ~~角色记录 8 槽字段访问器簇 `0x1B722`/`0x344F2`/`0x1BB8C`/`0x1B8E7`~~
   **已完成（§58，接入 85）**；
   ~~持久队伍记录表三件套 `0x1145A`/`0x11506`/`0x112A5`~~
   **已完成（§59，接入 88）**——转完 `funcs_25E23` 分派表 5 个表项的依赖闭包已闭合；
   ~~按 `funcs_25E23`（`0x51DE9`）表项顺序做状态 handler 族~~
   **已完成（§60，接入 93）**——`0x22EF6`/`0x231BC`/`0x23790`/`0x2389F`/`0x23E39` 五个表项
   （`scene_state_00/03/10/12/18`）进 `game/scene.c`，`scenecheck` 1100/0；
   其余 20 个表项依赖尚未转译，需先闭各自闭包。
   ~~按 `funcs_25E23` 表项顺序做状态 handler 族（续）~~
   **已完成（§61，接入 95）**——`0x239BD`(`[14]`，`scene_state_14`）+ 其唯一未转译依赖
   `0x33499`(`unit_exists`) 这个 2 函数 / 141 B 闭合小簇；`scenecheck` 1480/0、
   `reccheck` 37398/0。表内其余 19 项闭包均 > 3 KB（含 `0x122DC` 1051 B 等大函数），另开轮次。
   顺路按 `re/func_ranking.csv` 的用量/依赖拓扑收叶子（§51–§61）。
   ~~另一张独立分派表 `funcs_30469`（效果动画） 表项 `[4]/[7]/[8]/[9]`~~
   **已完成（§62，接入 99）**——`fx_dots6/dots3/dots16/toggle`（新模块 `game/fx.c`，`fxcheck` 3920/0）；
   ~~整表剩余 5 个 handler + 1 个 86 B 辅助 `0x2BF83`~~
   **已完成（§63，接入 104）**——`fx_dots7/dots8/blob/dots12/dots6b` + `fx_advance`
   （共 2749 B，`fxcheck` 3920→**12884/0**）；**整表 9/10 完成**，仅余 `[6] 0x2C67D`
   （1151 B，含 CRT `cos/sin`，另开一轮），转完可让小分派器 `sub_31266`（632 B）
   只差 `0x2FB2C`(744)+`0x2FE14`(237) 即闭合（`rounds/32` §62.6、`rounds/33` §63.7）。
   下一批同族候选 `0x1B8A6`/`0x1B83D`/`0x1CA89` 及记录单字节置位叶子
   `0x13512`/`0x32975`/`0x34D64`/`0x35009`（`rounds/28` §58.5）
   **已完成（§64，接入 111）**——7 个叶子进 `game/rec.c`，`reccheck` 37398→**42225/0**；
   排除 `0x205BE`（条目不合法，真入口 `0x205B4` 另评估）。
   下一批主候选：**`funcs_1199C`(`0x51B91`) 表项**（本轮只收 `[28]`/`[36]`，表里还有
   `[0]=0x34531`/`[1]=0x3460B`/… 这批同族 80 字节记录服务，依赖待逐个闭合）；
   `funcs_30469` 仍只差 `[6] 0x2C67D`。
   ~~对话框/头像合成三件套 `0x1956B`/`0x1974C`/`0x26996`（第 28 轮后置）~~
   **已完成（§65，接入 111→114）**——新模块 `game/msg.c`（624 B / 103 个调用点），
   `msgcheck` 465/0；`guest_mem` 堆缝是解锁条件。
   下一批主候选：**`funcs_1199C`(`0x51B91`) 表项**（同族 80 字节记录服务，依赖待逐个闭合）；
   `funcs_30469` 仍只差 `[6] 0x2C67D`（含 CRT 浮点）。
   ~~`funcs_1199C`(`0x51B91`) 表项的依赖闭合子集~~
   **已完成（§66，接入 114→125）**——11 个事件 handler（944 B，`game/ev.c/.h`，`evcheck` 2940/0）；
   表里其余 **34 项**全部挂在同一个约 **13 KB 的“场景渲染核”**（`0x10B4E`/`0x135DD`/`0x1366A`/
   `0x11CAC`/`0x11EEE`/`0x122DC`/`0x1ACF3` + 传递闭包），**这是下一个大里程碑，单独立项**。
   ~~“场景渲染核”里依赖已闭合的 4 个（`0x11EEE`/`0x24D22`/`0x122DC`/`0x1ACF3`）~~
   **已完成（§67，接入 125→129）**——地图视图渲染核 2589 B 进 `game/map.c`，`mapcheck` 112000/0。
   ~~`0x4E31C`（DAC 循环）+ `0x4E310`/`0x32230`/`0x11CAC`~~
   **已完成（§68，接入 129→133）**——调色板动画 + 地图视图刷新 496 B（`fade.c`/`map.c`），
   `mapcheck` 122500/0，窄 VEH `out` 陷阱落地（`PITFALLS` §8-83/84）。
   **下一步首选项：`0x135DD`(usage 98) 与 `0x1366A`(usage 110)**——两者依赖现已全闭
   （分别只剩本轮已转的 `0x11CAC`/`0x32230`），它们内部还有 VGA 模式/片头/滚动特例，
   各自成块、单独对拍；随后可推 `0x196CB`/`0x11AA8`/`0x11B48/9B/BFA/C59`，
   进而解锁 `0x197E5`(17 到达点) 与 `funcs_1199C` 剩余 34 项。
   记录维持：`repl.c` 加行 → `python tools/translation_map.py`（现 129→**133**/1359）。
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
   现状 111/1359 ≈ 8.2% 已源码化、其余仍是机器码且 C 会回调其固定 32 位地址 ⇒ 过渡期必须 32 位；
   路线（A 转译 → B 转完 → **C 去 guest 化** → D 64 位/三平台）见 `docs/TRANSLATION.md` §6、
   `rounds/16-entry-layer.md` §46.11。**下一步（工作重心）：按 §5 继续源码化**（51 → 1359）。
   同轮补：**Linux 窗口 180° 倒置修好**（GLSL 多翻一次 v；`--screenshot` 看不到窗口翻转，
   新增 `FD2_TESTPATTERN`/`FD2_GL_READBACK` 定向自检：修复前 `flipped=0`、修复后 `upright=0`，
   `PITFALLS` §8-67）；**转译记录/map** = `re/translation_map.csv` +`tools/translation_map.py`
   （`--check` 防漂移；81 wired / 1359）。
8. ~~FDPS（炎龙外传）~~ **已冻结**（2026-10-05 用户决定）：成果与卡点存档在 `docs/FDPS-ARCHIVE.md`，
   宿主的通用能力（`--exe`、FDPS AIL 表、定时器线程、INT9 注入）留在代码里不再主动维护。

## 5. 冻结 / 不再投入

| 项 | 状态 | 存档 |
|---|---|---|
| FDPS（炎龙外传） | 冻结（2026-10-05 用户决定） | `docs/FDPS-ARCHIVE.md` |
| SDL2 / SDL3 | 降为备选记录 | `docs/BACKEND.md` §13.2 / §13.3 |
| ~~鼠标 `INT 33h`~~ | 已判定不需要 | `docs/rounds/01-platform-and-tooling.md` §12.3 |
