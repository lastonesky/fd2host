# 第 43 轮：单位精灵构建链 + `funcs_1199C` 低索引收口（32 个，第 230–261 个）

> 操作者要求“继续”。本轮把 `docs/TRANSLATION.md` §5 排队的首选项一次做完：
> **`0x10B4E` 加载链**（`0x10B4E`+`0x10C50`+`0x11019`+`0x145CD`+`0x14625`+`0x1B750`）
> **+ `0x32999`** + **`funcs_1199C` 索引 0..37 剩余 25 个 handler**，共 **32 个**，
> 接入 229 → **261 / 1359（19.2%）**。`funcs_1199C` 整张表 0..90 至此**全部源码化**。

## 43.1 批次内容（32 个）

### 新模块 `src/game/unit_load.c/.h`（7 个，新分组 `REPL_UNITLD`）

| 原地址 | 大小 | 作用 |
|---|---|---|
| `0x10B4E` | 258 | 读 FDICON.B24 + FDFIELD.DAT，重建匹配的 80 字节记录，再写 FD2.TMP |
| `0x10C50` | 969 | 构建一条记录（最近空格搜索 + 模板/装备字段） |
| `0x11019` | 417 | 把 FDICON.B24 的第 idx 个精灵追加进 FD2.TMP 图集 |
| `0x145CD` | 88 | 按 `+6` 是否为 0 给记录所在的格子打 reveal 位 |
| `0x14625` | 130 | 揭示 (x,y) 及其四邻 |
| `0x1B750` | 237 | 8 个装备槽折叠进记录的包围盒 `+72/+74/+76/+78` |
| `0x32999` | 895 | 12 帧“单位出现”过场 |

### 新模块 `src/game/ev6.c/.h`（25 个，新分组 `REPL_EV6`）

`funcs_1199C` 索引 0..37 里尚未接入的 25 项：`0x34531`(0) `0x3460B`(1) `0x34673`(2)
`0x346CD`(3) `0x34778`(6) `0x350BE`(5) `0x350C8`(7) `0x34818`(9) `0x348BB`(11)
`0x34940`(14) `0x34984`(15) `0x349EC`(16) `0x34A1E`(17) `0x34B07`(20) `0x34B6F`(22)
`0x34B9A`(23) `0x34C52`(24) `0x34C7A`(25) `0x34D2F`(27) `0x34DD0`(30) `0x34EB3`(31)
`0x34F38`(32) `0x34FC2`(34) `0x34FCC`(35) `0x35022`(37)。

### harness

- **`src/ev2check.c`** 扩 batch-6：25 个 handler，新增 `0x32999` 记录桩（`EV_MAPR`）。
  `ev2check --cases=200 --only=<25 个>` = **5000/0**；全量 121 项 ×25 = **3025/0**。
- **新工具 `src/ev6check.c`**：加载链的机器码对拍。把 Watcom CRT 的
  `malloc/free/fopen/fclose/fseek/fread/fwrite` 七个入口重定向到宿主 libc
  （`rescheck` 的同款手术，多了 `fwrite`），再造一份合成 FDICON.B24（6 字节头 +
  140×48 字节表 + 140 段 blob）与 FDFIELD.DAT（LMI 容器，条目
  `3*dword_53C03+2` 里放模板字节）。A 组跑整条 `0x10B4E` 链，B 组直接对拍
  `0x1B750` 的随机记录。判据：`ev6check --cases=200` = **400/0**。

## 43.2 关键实现点

1. **FILE\* 不能跨 C/机器码边界**。`0x10B4E` 建 FDICON.B24 句柄 → `0x10C50` → `0x11019`；
   而 `0x11019` 还有 7 个机器码调用点（`0x10010`/`0x1088D`/`0x26152`/`0x2986F`/`0x2AA00`/
   `0x2AF28`/`0x2B843`）。宿主里机器码的 `FILE*` 是 **Watcom CRT 结构**，不是宿主 libc 的。
   所以 `unit_load.c` 全部走**游戏自己的 CRT 入口**（`0x37324 fopen`/`0x37940 fseek`/
   `0x373CA fread`/`0x377A3 fwrite`/`0x3759C fclose`）——宿主里是 Watcom（走 `dos.c` 的 int 21h），
   check 里被重定向成宿主 libc，两边一致。`res.c` 走宿主 stdio 是因为它只返回裸缓冲、没有
   FILE\* 过界。
2. **共享尾块 = 编译器尾合并**。IDA 的 `JUMPOUT(0x34885)`/`0x34663`/`0x34F65`/`0x34750`/
   `0x34C5C`/`0x35F6E` 都是跳进**别的函数中段**的 `vm_run`/置位尾块。C 里按各自的 `sub`
   参数内联成 `vm_play(sub)`，`0x35F6E` = `dword_51A83=1; return;`。`0x34FC2`/`0x350BE`/
   `0x34F38`/`0x34C52` 与 `0x34B07` 等互为同体。
3. **`0x11019` 读 13 个 dword，但记录步长是 48（12 个）**：`d[12]` 落在**下一条记录**的
   `d[0]`。这是原始格式，写合成 FDICON.B24 时必须容这个重叠（harness 一开始因
   `recs[140*48]` 少 4 字节而踩了返回地址）。
4. **图集头部前 1920 字节是 40×48 的槽表，blob 从 +1920 起**；未填的槽是 malloc 垃圾，
   对拍只能比已初始化区（`[0,48*BDF)` + `[1920,used)`）。
5. `0x1B750` 的 `__CHP` 是 Watcom 的 `fild; fmul 1.15; __CHP; fistp` 序列（`frndint` +
   截断控制字），C 侧就是 `(int)((double)v * 1.15)`；它还把 `+78` 一起写了（IDA 的
   共享尾声吞掉了这一笔）。

## 43.3 判据

```
ev2check --cases=200 --only=<25 个>：5000/0
ev2check --cases=25 全量：3025/0（121 项）
ev6check --cases=200：400/0
regress：all 8/8、none 8/8          repl: installed 229 → 261
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux 三对象 exact match、doscheck-linux 49/49
translation_map：261 wired / 1359（19.2%）
```

## 43.4 下一步（按 usage）

`funcs_1199C` 表已 91/91。余下按 `re/func_ranking.csv`：
`0x205DA`(28)、`0x197E5`(17)、`0x14818`(17)、`0x19953`(17)、`0x373CA`(17，CRT fread，
不必转)、`0x2FACD`(15)、`0x15F0E`(15)、`0x12CEA`(15)、`0x233C6`(15)、`0x1E0DB`(15)、
`0x1C4CC`(15)、`0x2C67D`(15，`funcs_30469[6]`，含 CRT cos/sin)。
解释器簇 `0x1AA1D` 的依赖也基本闭合（只差 `0x1B932`/`0x22AF6` 等零头）。
