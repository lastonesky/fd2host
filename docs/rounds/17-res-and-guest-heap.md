# 轮次明细：第 52 个转译函数 —— `res.c` 接入 + guest heap 缝
§47 解决"对拍早就过了但一直不敢接"的 `res.c`（原 `0x111BA`）：它跨 C / 机器码边界传缓冲，
两边必须用同一个堆。做法是抽出**唯一堆缝** `src/game/guest_mem.h`（宿主里=游戏的 Watcom CRT
堆 `0x3706E/0x3776E`，check 工具里已被重定向成宿主 libc，将来 64 位原生构建=宿主 malloc），
并让 `res.c` 把尺寸写进游戏全局 `0x53BFF`。接入分组 `res`（52 个），四项判据全过。
---

## 47.1 `res.c` 接入：guest heap 缝（第 52 个转译函数）

**背景**：`res.c`（原 `0x111BA`）对拍 160 例早就过了，但一直不敢接：它 `free(old_buffer)`
并返回一个 `buf`，两者都跨 C/机器码边界——用宿主 libc 的堆就会和游戏 Watcom CRT 的堆混用。

**IDA + 现有代码的结论**（`docs/TRANSLATION.md` §5 有完整版）：

- 游戏的堆入口是 **`0x3706E malloc` / `0x3776E free`**（`re/funcmap.csv` 的 `crt_sym`），
  底层 `_nmalloc/__MemAllocator/sbrk`。**不整体换 libc**：CRT 内部（stdio 的 `_ioalloc` 等）
  直接用自家堆，只换 `malloc/free` 会变成两堆混用。
- 先例：`game/dlg.c` 从第 30 轮起就用 `ORIG_ALLOC = 0x3706E` 分配、释放走原 `0x15E71`。

**落地**：

| 文件 | 内容 |
|---|---|
| `src/game/guest_mem.h/.c` | **唯一堆缝**：`guest_malloc/guest_free`（宿主里调 `0x3706E/0x3776E`；check 工具里这两个入口已被重定向成宿主 libc；非 x86 构建=宿主 `malloc/free`）+ `guest_store_u32/guest_load_u32`（写游戏全局） |
| `src/game/res.c` | 三处 `malloc/free` → `guest_malloc/guest_free`；尺寸不再写 C 变量，改 `guest_store_u32(0x53BFF, size)`（游戏读的是这个地址） |
| `src/rescheck.c` | `msz = res_size` → `msz = GUEST_SIZE`（在 C 调用后快照同一地址）——仍能区分"原版写的/翻译写的" |
| `src/repl.c` / `repl.h` | 新分组 `REPL_RES 0x200`/`--replace=res`，表里加 `{ 0x111BA, "res_load", … }` |

**判据（本轮实测）**：

| 判据 | 结果 |
|---|---|
| `rescheck` | **160 cases, 0 failures** |
| `regress.ps1` | **8/8 PASS**，`FD2.TMP = 207360` |
| `repl: installed` | **52** translated function(s)（mask 0x3FF） |
| A/B 同 tick（`--replace=none` vs `all`） | **0 / 64000 px** |
| Linux `host32` 同 tick vs Windows | **0 / 64000 px**（guest_mem 在 Linux 32 位下同样走游戏堆） |
| `translation_map.py --check` | 52 wired，up to date |

**顺带**：`--replace` 分组文档、AGENTS/ENVIRONMENT 更新到含 `res`（并踩到 `PITFALLS` §8-68）。
剩 `0x15E71`/`0x15E9E`：快照缓冲改走 `guest_mem` 后再接；`dlg.c` 的 `ORIG_ALLOC` 逐步迁移。
