# 轮次明细：主状态机族第二刀 —— `0x22E5C` scene_card 转译（第 56 个转译函数）
§50 顺着 `main`（`0x25BF4`）的调用图取了族里最小的一个：`0x22E5C`（154 B，纯服务序列）。
同轮把两张**分派表**的真实地址钉死（`funcs_25E23 @0x51DE9`、`funcs_25E3A @0x51D71`），
为后续状态 handler 的排期提供依据。`scenecheck` 100 例一次通过；`regress` 曾出现一次
"未达退出条件"的偶发（重跑 8/8，见 §50.4）。
---

## 50.1 侦察：`main` 与两张分派表（IDA）

`main` 就是 **`0x25BF4`**（711 B），骨架：

```
AIL_startup / AIL_install_MDI_INI -> dword_53ED8, byte_53EF0, dword_53ED0
AIL_install_DIG_INI               -> dword_53EDC, byte_53EF1, dword_53EE4/8
res_load(FDOTHER.DAT, …) × 7、res_load(FDTXT.DAT,0)、malloc(32/…/2560)、int386(16h) 读键
while (1):
    sub_25EBB()                       ; 进/退状态
    if (0) do { sub_117E7();          ; 每帧
                 if (dword_53ECC==1) { sub_22E5C(); }        ; 场景卡
                 else if (dword_53ECC==2) { bgm_play(-1,1);
                        funcs_25E23[dword_53C03]();
                        sub_26152();    ; 状态推进
                        if (0) { funcs_25E3A[dword_53C03]();
                                 bgm_play(byte_51E63[state], 0); } }
               } while (!done)
```

两张表（本轮从 `call funcs_XX[eax*4]` 的 disp32 反算并 dump）：

| 表 | 地址 | 前几项 |
|---|---|---|
| `funcs_25E23[]` | **`0x51DE9`**（obj2 数据） | `0x22EF6 0x22F37 0x230F2 0x231BC 0x231F9 0x23296 0x232E8 …` |
| `funcs_25E3A[]` | **`0x51D71`** | `0x3231B 0x32D18 0x32E8C 0x32FB2 0x33049 0x3314B …` |

> 更正：RE_MAP 里 `funcs_25E23`/`funcs_25E3A` 曾被当成"局部表/地址待提取"，现已是确切地址。

## 50.2 转译 `0x22E5C` → `scene_card`

反汇编（154 B，`dword_53ECC==1` 时进）：

```
bgm_play(-1, 1); svc_wait_ticks(1); fade_out();               ; 0x1F882 变暗
buf = res_load("FDOTHER.DAT", NULL, 79);
memset(0xA0000, 0, 64000);
rle_blit(buf, 0, 0xA0000, 320, -1);                            ; 0x2EB9F
fade_in(); svc_wait_ticks(9);                                  ; 0x1F525 变亮
rle_blit(buf, 1, 0xA0000, 320, -1);
svc_wait_ticks(36);
free(buf);                                                     ; 尾是 `push ebx; jmp loc_15E94`
```

`src/game/scene.c` 逐行照搬；服务**经原地址**调用（`0x25977`/`0x17AA9`/`0x111BA` 在宿主里
已分别换成 `bgm.c`/`svc.c`/`res.c` 的 C），缓冲用 `guest_free` → 与 `res_load` 同一堆。
`--replace` 新分组 `scene`，接入第 **56** 个函数。

**顺带澄清的小函数**（未转译，本轮只记录语义）：
- `0x1F882`（18 B）= 调 `0x1F525` 的循环体：`i=0..63 { pal_sub(0,255,i); delay(2); }`（变暗）
- `0x1F525`（51 B）：`i=64..0 { pal_sub(0,255,i); delay(2); }`（变亮）
- `0x2EB9F`（66 B）= 按偏移表取子图 → 调**已转译**的 `rle_decode`（`0x4E98D`）
- `0x11D40`（178 B）= 调色板区间渐变（`outp 0x3C8/0x3C9`，`dword_53A65` 里减 `sub`，下限 0）

## 50.3 判据

| 判据 | 结果 |
|---|---|
| `scenecheck` | **100 cases, 0 failures**（把 7 个服务 hook 成记录桩，逐调用比对 序列+参数） |
| `regress.ps1` | **8/8 PASS**，`FD2.TMP=207360` |
| `repl: installed` | **56**（mask 0xFFF） |
| A/B 同 tick `--replace=none↔all` | **0 / 64000 px** |
| Linux `host32` 同 tick ↔ Windows | **0 / 64000 px** |
| `translation_map.py --check` | **56 wired / 1359（4.1%）** |

## 50.4 一次偶发，按"待确认"记录

本轮第一次 `regress` 失败：日志显示游戏正常跑到 **60 s 上限**、音乐/存档/帧数都正常，
但 `--exit-when-file=FD2.TMP:207360` 没触发（4 项检查失败）。**原样重跑即 8/8**，
且 `--replace=none↔all` 同 tick 0 px。之前（第 47 轮 A/B）也遇到过一次 `--replace=all`
启动早期就在系统 DLL 里 ACCESS VIOLATION 的一次性现象。两次都未能复现，暂**标"待确认"**：
不像本轮改动引入（scene 只在 `dword_53ECC==1` 走，A/B 0 px），疑似窗口/定时器/焦点类启动竞争
（参考 `PITFALLS` §8-57）。后续跑稳定性长跑时重点盯。

## 50.5 下一步

状态机 handler 可一条一条来；建议按**表项顺序**从 `funcs_25E23[0] = 0x22EF6` 起（与 `0x22E5C`
相邻、同一场景族），或先转 §50.2 里那四个小工具（`0x2EB9F`/`0x1F882`/`0x1F525`/`0x11D40`）
以消掉 `scene_card` 里剩下的机器码回调。记录维持：`repl.c` 加行 → `python tools/translation_map.py`。
