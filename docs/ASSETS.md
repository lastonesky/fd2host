# 资产导出（把游戏资源转成现代格式）

> 目标：游戏内的资源（音乐、人物造型、动画、场景、文本）在**游戏外**也能用，
> 导出成 PNG / MIDI / WAV / JSON 等现代格式。
> 格式知识来自 `docs/knowledge-base/01..09`（当年的逆向成果，已用真实文件+画面验证），
> 工具是 **`tools/fd2assets.py`**（纯 Python，无需构建，只用标准库 `zlib`）。

## 1. 工具用法

```bash
python tools/fd2assets.py list    E:/FD2/FDOTHER.DAT          # 看容器里有什么
python tools/fd2assets.py unpack  E:/FD2/FDOTHER.DAT --recurse   # 解包（含嵌套 TITLE.DAT）
python tools/fd2assets.py image   E:/FD2/BG.DAT 3 --gamedir E:/FD2 --out bg003.png
python tools/fd2assets.py dato    E:/FD2/DATO.DAT  --gamedir E:/FD2   # 人物头像 4 帧 -> PNG
python tools/fd2assets.py tileset E:/FD2/FDSHAP.DAT --gamedir E:/FD2  # 地形图块 -> PNG sheet + JSON
python tools/fd2assets.py music   E:/FD2/FDMUS.DAT                # XMIDI -> .mid
python tools/fd2assets.py all     E:/FD2 --out build/assets       # 以上全部
```

`all` 的实测产出（2026-10-07，`build/assets/`，不入库）：

| 产物 | 数量 | 判据 |
|---|---|---|
| 容器解包 | 12 个 DAT（含嵌套 `FDOTHER` 104→310 文件、`FIGANI` 409） | 每个资源落盘 + `manifest.json` |
| 全幅图 PNG | **68**（BG/标题/战斗背景/杂项 UI） | `TITLE_000.png` = 标题 logo；`BG_003.png` = 山脉背景 |
| 人物头像 PNG | **545**（137 肖像 × 4 嘴型帧） | `DATO_000_0.png` = 索尔侧脸 80×80 |
| 地形图块 | **33 tileset PNG**（16 列 sheet）+ 33 张地形控制表 JSON | `FDSHAP_000.png` = 草地/水/山/路/屋，288 格 24×24 |
| 音乐 | **15 首 .mid** | `MThd`，format 1，事件表与 XMIDI 一致 |

## 2. 已实现的格式（都按 `knowledge-base` + 已验证解码器，非猜测）

| 类别 | 规格与判据 | 产物 |
|---|---|---|
| 容器 | `"LLLLLL"` 魔数 + `+6` 起 u32 LE 偏移表，`N=(offsets[0]-6)/4`；嵌套容器递归 | 原样落盘 + `manifest.json` |
| 调色板 | `FDOTHER.DAT` 资源 0 = 256×RGB 6-bit（≤0x3F）×4 | PNG 的 `PLTE` |
| 全幅图 | `+0` u16 LE w/h；`len-4==w*h` 未压缩，否则 RLE（`c>=0x80` 取 `(c&0x7F)+1` 字面，`c<0x80` 重复 `c+1`） | 8-bit 调色板 PNG |
| **DATO 头像** | 资源开头 `u32[4]` 帧偏移；每帧 `u16 W,H` + RLE（`b<=0xC0` 一个字面像素值 `b`，`b>0xC0` 下一字节重复 `b-0xC0` 次） | 每肖像 4 个 PNG（80×80）+ `portraits.json` |
| **FDSHAP 图块** | 资源头 `u16 tileW(=24), u16 tileH(=24), u16 count` + `u32[count]` 偏移，图块是 **sprite24 四模式 RLE**（`tok>>6`：0 实心 run / 1 隔像素 run / 2 字面 / 3 特殊），每块精确 24×24 | 每 tileset 一张 16 列 PNG sheet + JSON；奇数资源 = 地形控制表 → JSON |
| 音乐 | XMIDI（IFF `FORM XDIR/XMID` + `TIMB`/`EVNT`）；延迟是 `<0x80` 字节累加，note-on 后跟 VLQ 时长（需自行排 note-off） | 标准 MIDI（`MThd`/`MTrk`，VLQ、补 note-off、`FF 2F`） |

**FDSHAP 用的是哪套 RLE（实测澄清）**：`knowledge-base/01` §8 写"RLE 同 §2"，但实测
**33 个 tileset × 全图块**用 **sprite24 四模式**（`tok>>6`）**精确解满 24×24 且恰好用尽流**，
用 §2 那套会在大量图块上失败。所以导出走四模式；颜色映射这里用 **identity**——
游戏可能按 tileset 用 ramp/palette 模式给地形重新上色，导出器不去猜（原始索引已足够重制时自行映射）。

## 3. 还没接的（格式也都在 knowledge-base 里）

| 资产 | 容器/资源 | 规格 | 需要的解码器 |
|---|---|---|---|
| 战斗招式/法术动画 | `FIGANI.DAT`（409 资源） | `06` | 每帧 13 字节头 + 4 模式 RLE（`sprite24.c` 已对拍）+ 帧时间轴 → PNG 帧序列 + JSON |
| 过场/片头动画 | `ANI.DAT`（AFM 容器，10 资源） | `06` | AFM 帧封装 |
| 24×24 地图图标 sprite | `FDICON.B24`（1680 个） | `01` 勘误 | four-mode sprite RLE（同上，机制已具备） |
| 文本/对白 | `FDTXT.DAT`（35 资源） | `08` | 自制字型 glyph 索引（13/16 点阵）→ UTF-8 |
| 音效 PCM | `.DIG` / `SAMPLE.*` | `04`/AIL | 8-bit PCM → WAV |
| 每屏调色板 | 各容器可能自带 | `01` §3 | 目前统一 `FDOTHER#0`，后续按屏切 |
| 地图成品图 | `FDFIELD.DAT` 3 资源/地图 + 配对 tileset | `03`/`08` | 按 `地形索引→图块` 合成 地图 PNG（`render_map` 那类） |

## 4. 为什么不做成 C 工具

解码器在 C 里已对拍（`rle.c`/`sprite24.c`/…），但导出是**离线一次性**的事：Python 零构建、
零依赖（`zlib` 标准库）、改起来快。等 §3 的 codec 都补齐、需要与游戏解码逐字节强对齐时，
再把关键解码器抽成 C 工具复用也不迟（`docs/TRANSLATION.md` §6 阶段 C 本来就要把解码器留在 C 侧）。
