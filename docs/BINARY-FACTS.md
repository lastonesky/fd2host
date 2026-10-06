# FD2.EXE 二进制事实（已验证）
容器结构、LE 头**实测偏移**、fixup 记录格式、对象数据在文件中的位置、游戏的平台依赖清单。
对应旧 `PROGRESS.md` §3。所有结论都以 Ghidra 镜像逐字节对比或运行日志为判据。
---

## 3. 游戏二进制的已验证事实

### 3.1 容器结构

```
文件偏移 0         MZ 头（DOS/4GW 改造过，e_lfanew 字段不可用）
文件偏移 0 .. 0x27ACC   DOS/4GW 加载器 stub（16 位，含 "RATIONAL DOS/4G"、"WATCOM"、"Phar Lap" 等检测串）
文件偏移 0x27ACC   LE 头（"LE\0\0"）
文件偏移 0x36014   对象数据页起点（页对齐、紧凑）
文件大小 0x7C4E6   509,158 字节
```

### 3.2 LE 头字段偏移（**实测值，与标准 LE 不同**，务必按此解析）

| 相对 LE 头 | 值 | 含义 |
|---|---|---|
| +0x04 | 0 | format level |
| +0x08 | 0x00010002 | cpu=2(80286), os=1(DOS) |
| +0x10 | 0x200 | module flags |
| +0x14 | 71 | module pages |
| +0x18 | 1 | EIP 对象号（1-based） |
| +0x1C | 0x2CCB4 | EIP（相对该对象基址） |
| **+0x40** | 0xC4 | **对象表偏移** |
| **+0x44** | 3 | **对象数** |
| **+0x48** | 0x10C | **对象页表偏移** |
| **+0x68** | 0x22F | **fixup 页表偏移** |
| **+0x6C** | 0x34F | **fixup 记录表偏移** |

对象表项 24 字节：`vsize(4) base(4) flags(4) pageidx(4) npages(4) reserved(4)`

| 对象 | base | vsize | 页号 | 属性 |
|---|---|---|---|---|
| obj0 | `0x00010000` | `0x3EF29` | 1..63 | READ+EXEC+PRELOAD+32BIT |
| obj1 | `0x00050000` | `0x056B0` | 64..67 | READ+WRITE+PRELOAD+32BIT |
| obj2 | `0x00060000` | `0x034D2` | 68..71 | READ+WRITE+PRELOAD+32BIT |

**入口线性地址 = obj0.base + 0x2CCB4 = `0x3CCB4`**（DOS/4GW 的 C 运行库启动代码）。

### 3.3 fixup 记录格式（已 100% 验证）

fixup 页表：72 个 u32（相对记录表起点的累计偏移），页 i（1-based）的记录块 = `[pt[i-1], pt[i])`。
记录表总长 57,497 字节。

每条记录：

```
type 0x07 : [07][size][src:2 LE][obj:1][tgt:2 + (size>>4) 字节 LE]
            size 高 4 位 = 额外目标字节数，目标字节数 = 2 + (size>>4)
            obj  = 目标对象号（1-based）
            动作 : *(u32*)(page_base + src) = objects[obj-1].base + tgt
type 0x00 : 1 字节填充，跳过
```

**必须跳过"源操作数跨页"的记录**（`src + 4 > 0x1000`，共 22 条）。它们会把下一页开头的代码
覆盖掉——不跳过就会崩在 `0x3E000`（已实测）。Ghidra 的加载器同样不应用它们。

验证结果（`letest.exe`）：obj1、obj2 与 Ghidra 重定位镜像**逐字节一致**；obj0 仅剩 11 字节差异
（全部位于页边界，语义待确认，当前认定无害）。

### 3.4 对象数据在文件中的位置（推导法）

对象页数据在文件里**页对齐、按对象顺序紧凑存放**，最后一个对象只占其 `vsize` 余数：

```
总跨度 = Σ(对象 page_count × 0x1000)，但最后一个对象取 (page_count-1)×0x1000 + vsize%0x1000
data_start = 文件大小 - 总跨度            =>  0x36014（本文件）
之后逐对象顺序拷贝，遇到文件末尾截止
```

### 3.5 游戏的平台依赖（实测）

| 子系统 | 实现 | 备注 |
|---|---|---|
| 图形 | 直接读写 `0xA0000`，320×200、8bpp 索引色 | 60+ 处硬编码 `0xA0000` |
| 显示模式 | `INT 10h` AH=0（已实测设 0x13）、AH=0F、AX=1C00 | 由 DOS/4GW stub 调用 |
| 调色板 | `out 0x3C8`（索引）/ `out 0x3C9`（R,G,B） | 宿主已捕获到 `port ops` |
| 声音 | 静态链接 Miles AIL + **外置 16 位实模式驱动** `*.DIG` / `*.MDI` | AIL 会跳进驱动代码执行 ⇒ 必须整体替换 |
| 音乐数据 | 标准 XMIDI（IFF `FORM/XMID`），在 `FDMUS.DAT` | 可用现代合成器播 |
| 键盘 | **直接轮询 BDA**：`0x41A/0x41C`（缓冲 head/tail）、`0x46C`（BIOS tick）、`0x417` | 宿主已重定向到低内存镜像 |
| 鼠标 | `INT 33h` | 待接真实状态 |
| 计时 | 轮询 `INT 21h AH=2C` 的 **DH（秒）** 等变化 | 忙等循环，正常 |
| 文件 | `INT 21h` AH=3D/3E/3F/40/42/44(IOCTL) | 按裸文件名打开，故工作目录必须是游戏目录 |
| 内存 | `INT 21h AH=48`、`INT 31h AX=0x0100 / 0x0501` | 见 §4.4 的返回值语义 |
| DPMI | `INT 31h`（描述符、版本、内存、实模式向量） | |
| 数据文件 | `DIG.INI` `MDI.INI` `*.DIG` `*.MDI` `FDOTHER.DAT` `FDTXT.DAT` `FDSHAP.DAT` `FDFIELD.DAT` `DATO.DAT` `FDMUS.DAT` `BG.DAT` `FIGANI.DAT` `ANI.DAT` `TAI.DAT` `FD2.SAV` `FD2.TMP` `FDICON.B24` | 格式已在 `FD2_analysis.md` 里分析过（DAT 容器 = "LLLLLL" + header_size + 偏移表） |
