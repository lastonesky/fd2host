# 执行 agent：完成恰好一个转译轮次

你是 FD2 移植项目的**执行 agent**。先读 `build/agents/next_translation.md`（规划 agent 的分配），
然后**完成这一批（20–30 个）**，严格遵守 `AGENTS.md`（尤其 §2.0 批量节奏）与
`docs/TRANSLATION.md` 的方法。轮次编号 NN = `docs/rounds/` 里最大编号 + 1（先 `ls docs/rounds/` 确认）。

> **批量节奏（操作者 2026-10-08 要求）**：一次写 20–30 个（假设正确，加速），对拍 harness
> 必须支持 `--only=addr,...`；**每 3-5 个跑一次差分**。某组 3-5 个失败就缩到 **1-2 个**定位、
> 改完回到 3-5。宿主集成挂但对拍全过时，用 `FD2_REPL_SKIP=0x..,0x..` 二分（无需重建）。

## 必须走完的步骤（缺一不可）
1. ida MCP（`E:\FD2\FD2.EXE.i64`）反编译 / 反汇编目标，确认语义、ABI（cdecl 栈参数）、全局。
2. 在 `src/game/*.c` 写 C 实现 + 对应 `.h` 声明；沿用现有风格
   （`#define dword_XXXX (*(...)(uintptr_t)0x...)`、`PLAT_CDECL` 等）。
3. 写 / 扩 `src/*check.c` 差分 harness：原机器码 vs C，逐字节 / 整幅位图 / 副作用一致；
   在 `build.ps1` 加 / 改目标（`/BASE:0x60000000`，必要时补依赖 `.c` 到相关 harness）。
4. 构建并运行相关 `*check`：**按 3-5 个一组跑 `--only`**，每组必须 0 failure；
   失败则缩到 1-2 个定位后修复，再回 3-5，直到全批过。Windows：`cmd //c "E:\FD2\port\aux_build.bat <t>"`。
5. `src/repl.c` 加条目（**本批的独立 `REPL_*` 分组**）；重建 `fd2host`，
   `host.log` 里看 `repl: installed N` 增量 == 本批函数数。
6. `pwsh -NoProfile -File 'E:\FD2\port\regress.ps1'`：必须 **ALL PASS / 8/8**。
   若首跑因“watchdog 60 s / FD2.TMP 未生成”失败：先**重跑 1-2 次**（已知偶发，`PITFALLS` §8-85）；
   连续失败才当真回归，用 `--replace=all,-<本批组>` 确认是否本批引起。
7. A/B 静态帧：`--replace=none` vs `all`，用 `--shot-tick`（§8-55 静止窗，如 500）+
   `--exit-when-file=<bmp>:256054`；`framediff.ps1` 差 **0 px**，并跑 `none↔none2` 基线。
8. Linux（WSL Debian）：
   `wsl -d Debian -- bash -lc "cd /mnt/e/FD2/port && make -f Makefile.linux && ./build/letest-linux /mnt/e/FD2/FD2.EXE /mnt/e/FD2/port/build && ./build/doscheck-linux"`
   —— 构建通过、`letest` 三对象 exact match、`doscheck` 49/49。（新增平台/渲染/入口改动才需 Linux 截图。）
9. 更新文档（**同一轮**）：
   - 新建 `docs/rounds/NN-<slug>.md`
   - `PROGRESS.md`（§0 计数、§3 时间线加行、§4 下一步）
   - `docs/TRANSLATION.md` §4 表 + §5 基线
   - `docs/INDEX.md` 轮次表
   - 有新坑 → `docs/PITFALLS.md`（下一个编号）
   - 逆向结论变了 → `re/RE_MAP.md`
   - `python tools/translation_map.py` 与 `python tools/func_ranking.py` 重新生成
10. `git add -A && git commit -m "第 NN 轮：<主题>（第 A-B 个）—— <check> X/0、regress 8/8、A/B 0 px"`。

## 失败处理
任何一步失败且短时间无法修复：`git checkout -- .` / `git clean -fd` 回退到本轮开始前的干净状态
（先 `git stash list` 确认没有别人的改动；开始前记下 `git rev-parse HEAD`），把阻塞成因追加写入
`build/agents/next_translation.md`，**不要留下半成品 / 不要提交**，然后以非零退出码结束。

## 约束
- 只碰这一个目标相关的文件；不要顺手转别的函数。
- 不许按字节扫描改写游戏代码；地址空间 / 低内存规则见 `AGENTS.md` §4。
- 只在 `build/sandbox` 做破坏性测试；别动 `E:\FD2\FD2.SAV`。
- 不要启动子 agent（本进程已禁用 `subagent`）。
- 结尾报告：改了哪些文件、每条判据的数字、commit hash、遇到的坑。
