# 轮次明细：快照对 `0x15E9E`/`0x15E71` 转译（第 53/54 个转译函数）
§48 把 `dlg.c` 里最后两处"对拍早就过了但一直在调机器码"的调用点换成 C：
人像滑入/收框动画用的 VGA 矩形快照 `snap_save`（`0x15E9E`）与 `snap_restore`（`0x15E71`）。
关键是**堆**：`0x15E71` 有大量未转译的机器码调用者，快照缓冲必须继续留在游戏的 Watcom 堆上，
所以新代码走 `guest_mem`（§47 建立的缝）。
---

## 48.1 反汇编结论（`re/dlg_disasm2.txt`）

**`0x15E71`（45 字节，`snap_restore`）**：cdecl 三参 `(record, surface, stride)`：

```
push stride; push surface; push record
call 0x4EC7C            ; gfx_restore_rect
push record; call 0x3776E  ; free
```

⇒ `gfx_restore_rect(record, surface, stride); free(record);`

**`0x15E9E`（`snap_save`）**：cdecl 五参 `(block, surface, stride, x, y)`：

```
edi = block;  w = (int16)[edi];  h = (int16)[edi+2]     ; movsx
off = y*stride + x
malloc(w*h + 8)                                          ; 0x3706E
gfx_save_rect(rec, w, h, surface, off, stride)           ; 0x4ECBF, 写 8 字节头
gfx_blit_transparent(surface+off, block, stride)         ; 0x4ED34
return rec
```

`gfx_restore_rect` 从记录头里读回 `offset`，所以 `snap_restore` 不需要 x/y —— 与
`rescheck`/`gfxcheck` 已对拍过的 `gfx_save_rect`/`gfx_restore_rect`/`gfx_blit_transparent`
语义完全吻合（`src/game/gfx.h`）。

## 48.2 实现与接入

| 位置 | 改动 |
|---|---|
| `src/game/dlg.c` | 新增 `dlg_snap_save`/`dlg_snap_restore`（走 `guest_malloc/guest_free` + 已转译的 gfx 函数）；`dlg_open_box`/`dlg_close_box` 不再调 `ORIG_SNAP_*`，删掉那两个函数指针与宏 |
| `src/game/dlg.h` | 声明两个新函数 |
| `src/repl.c` | 分组 `dlg` 增加 `{0x15E9E, snap_save}` / `{0x15E71, snap_restore}`（共 **54**） |
| `build.ps1` | `dlgcheck`/`boxcheck`/`keycheck`/`typecheck` 加 `game\guest_mem.c` |

**为什么 patch `0x15E71` 是安全的**：它的调用者里有 `sub_10010`/`sub_1A30B`/`sub_1E98C`/
`sub_1EB05`/`sub_1F42D` 等**未转译**函数，它们的记录由原 `0x15E9E`/`0x15F0E` 用 Watcom
`malloc` 分配；C 版 `guest_free` 走的正是同一个 `0x3776E`，所以两边可以互相 free。
反向同理：C 版分配的记录也能被原机器码 free。

## 48.3 判据（本轮实测）

| 判据 | 结果 |
|---|---|
| `boxcheck` | **240 cases, 0 failures**（这是快照对的差分判据：C 版 `dlg_open_box` 用 C 快照，原版 `0x165AC` 用机器码快照，整帧 VGA + 事件序列对比） |
| `dlgcheck` / `typecheck` / `keycheck` | **800/0** / **1616/0** / **100/0** |
| `regress.ps1` | **8/8 PASS**，`FD2.TMP=207360` |
| `repl: installed` | **54** translated function(s)（mask 0x3FF） |
| A/B 同 tick `--replace=none` ↔ `all` | **0 / 64000 px** |
| Linux `host32` 同 tick ↔ Windows | **0 / 64000 px** |
| `translation_map.py --check` | **54 wired / 1359（4.0%）** |

## 48.4 下一步

`dlg.c` 里已无"故意留机器码"的调用点。下一个大目标是**主状态机**
（`0x25977`/`0x25EBB`/`0x117E7`/`0x22E5C`/`0x26152`），见 `docs/TRANSLATION.md` §5。
记录维持：`repl.c` 加行 → `python tools/translation_map.py`。
