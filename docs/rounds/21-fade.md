# 轮次明细：主状态机族的调色板淡入淡出三件套（第 57–59 个转译函数）
§51 把 `scene_card` 里剩下的调色板淡变回调转掉：`0x11D40`（区间淡变）、`0x1F882`（变暗）、
`0x1F525`（变亮）。它们被 `0x22E5C` 和 **29 个其他调用点**使用（含 `main` 区域），所以这一刀
覆盖的不只是一个函数。`fadecheck` **4000/0**；顺带修掉了对拍工具自身的一个数组越界。
---

## 51.1 反汇编结论

**`0x11D40(start, end, sub)`**（178 B）：

```
while (start <= end) {
    outp(0x3C8, start);                                  // DAC 写索引
    for k in 0..2:
        v = pal[start*3+k] - sub;  if (v < 0) v = 0;      // 字节 - sub，下限 0
        outp(0x3C9, v);
    start++;
}
```
`pal` = `*(uint8_t**)0x53A65`（游戏自己的调色板副本，3 字节/项）。每个索引 4 次端口写。

**`0x1F882`（18 B）**：`ebx=0` → 跳到共享循环 `0x1F503`：`fade_range(0,255,i); delay(2); i++`，
`i<0x40` ⇒ **sub 从 0 增到 63 = 逐步变暗**（fade out）。

**`0x1F525`（51 B）**：`ebx=64` → `fade_range(0,255,i); delay(2); i--`，`i>=0`
⇒ **sub 从 64 减到 0 = 逐步变亮**（fade in）。

⇒ `src/game/fade.c`：`pal_fade_range/pal_fade_out/pal_fade_in`；`outp`(0x37AE5) 与
`delay`(0x3790A) **仍按原地址调用**（DAC 端口和 BIOS tick 归 `dos.c`，对拍时被 hook）。
`--replace` 新分组 `fade`，接入 **57/58/59**（`repl: installed 59`，mask 0x1FFF）。

## 51.2 对拍工具自身的一个越界（记下，避免再踩）

`fadecheck` 第一版把每个 `outp` 存进 `ev_t g_ev[1024]`。算错了量级：**一次完整淡变**
= 256 个索引 × 4 次端口写 = **1024** 个事件/步，而 `fade_in/out` 有 65 步 ⇒ **~66k 事件**。
数组越界后比较读到垃圾，报 `outp[1025] (1,0) vs (686EC10F,…)`。
修法：不存全序列，改存 **FNV-1a 哈希 + outp/delay 计数**（序列完全确定，(count,hash) 足够，
内存 O(1)）。判据仍是"逐调用等价"，只是压缩了表示。

## 51.3 判据

| 判据 | 结果 |
|---|---|
| `fadecheck` | **4000 cases, 0 failures**（三种模式：随机区间/淡出/淡入；随机调色板保证触发下限截断） |
| `regress.ps1` | **8/8 PASS**，`FD2.TMP=207360` |
| `repl: installed` | **59**（mask 0x1FFF） |
| A/B 同 tick `--replace=none↔all` | **0 / 64000 px**（淡变直接影响调色板，0 px 说明端口写序列一致） |
| Linux（按新分级：只做构建+自检） | `make` = 0 warning；`letest` exact match；`doscheck` **49/49** |
| `translation_map.py --check` | **59 wired / 1359（4.3%）** |

> 本轮起执行 `AGENTS.md` §2.6 的新分级：纯游戏逻辑转译**不再每轮拍 Linux 截图**，
> 只跑构建 + `letest`/`doscheck`（秒级）；Linux 截图留到动平台/渲染/入口层的轮次与里程碑。

## 51.4 下一步

`scene_card` 里只剩 `0x2EB9F`（LMI 子图 RLE blit，66 B）是机器码回调；它调用已转译的
`rle_decode`、需要按偏移表取子图（`buf + *(u32*)(buf+4*index+8)`）。
之后按 `funcs_25E23[0]=0x22EF6` 起的顺序继续状态 handler。
