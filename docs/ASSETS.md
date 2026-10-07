# 资产导出（把游戏资源转成现代格式）

> 目标：游戏内的资源（音乐、人物造型、动画、场景、文本）在**游戏外**也能用，
> 导出成 PNG / MIDI / WAV / JSON 等现代格式。
> 格式知识来自 `docs/knowledge-base/01..09`（当年那份逆向成果，已用真实文件+画面验证过），
> 工具是 **`tools/fd2assets.py`**（纯 Python，无需构建）。

## 1. 工具用法

```bash
# 看一个容器里有什么
python tools/fd2assets.py list   E:/FD2/FDOTHER.DAT

# 解包（--recurse 会钻进嵌套容器，如 FDOTHER 里的 TITLE.DAT）
python tools/fd2assets.py unpack E:/FD2/FDOTHER.DAT --out build/assets/fdother --recurse

# 单张全幅图 -> PNG（调色板取 FDOTHER.DAT 资源 0）
python tools/fd2assets.py image  E:/FD2/BG.DAT 3 --gamedir E:/FD2 --out build/assets/bg003.png

# 音乐：FDMUS.DAT 里的 XMIDI -> 标准 .mid
python tools/fd2assets.py music  E:/FD2/FDMUS.DAT --out build/assets/music

# 一键：解包全部 DAT + 导出能确定的图/乐
python tools/fd2assets.py all    E:/FD2 --out build/assets
```

`all` 的实测产出（2026-10-07）：**12 个容器全解包**（含嵌套：`FDOTHER` 104→310 个文件、
`FIGANI` 409），**70 张全幅图 → PNG**（BG/标题/战斗背景/杂项 UI），**15 首 XMIDI → MID**。
画面判据：`TITLE_000.png` = 标题 logo「FLAME DRAGON 2 — Legend of Golden Castle」，
`BG_003.png` = 山脉战斗背景（都是 320×200 / 320×100）。

## 2. 已实现的格式（都按 `knowledge-base` 的规格，非猜测）

| 类别 | 规格 | 产物 |
|---|---|---|
| 容器 | `"LLLLLL"` 魔数 + `+6` 起 u32 LE 偏移表，`N=(offsets[0]-6)/4` | `unpack` 原样落盘 + `manifest.json`；嵌套容器自动递归 |
| 调色板 | `FDOTHER.DAT` 资源 0 = 256×RGB，每通道 6-bit（≤0x3F），×4 转 8-bit | PNG 的 `PLTE` |
| 全幅图 | `+0` u16 LE w/h；`len-4==w*h` 为未压缩，否则 RLE：`c>=0x80` 取 `(c&0x7F)+1` 个字面字节，`c<0x80` 下一个字节重复 `c+1` 次 | 8-bit 调色板 PNG（zlib 手写，无第三方依赖） |
| 音乐 | XMIDI（IFF `FORM XDIR/XMID` + `TIMB`/`EVNT`）；延迟是 `<0x80` 字节**累加**，note-on 后跟 VLQ 时长需自行排 note-off | 标准 MIDI（`MThd`/`MTrk`，VLQ 增量、补 `note off`、`FF 2F` 收尾） |

## 3. 还没接的（下一步，格式也都在 knowledge-base 里）

| 资产 | 容器/资源 | 规格文档 | 需要的解码器 |
|---|---|---|---|
| 人物头像（对话嘴型 4 帧 80×80） | `DATO.DAT`（137 资源） | `01` §7 | 「高值-run RLE」另一套 token |
| 24×24 地图/图标 sprite | `FDICON.B24`（1680 个）、`FDSHAP.DAT` 的 tileset | `01` §2/§8 | four-mode sprite RLE（**已转译** `src/game/sprite24.c`） |
| 战斗招式/法术动画 | `FIGANI.DAT`（409 资源） | `06` | 每帧 13 字节头 + 4 模式 RLE（**部分已转译** `sprite24.c`） |
| 过场/片头动画 | `ANI.DAT`（AFM） | `06` | AFM 帧封装 |
| 文本/对白 | `FDTXT.DAT`（35 资源） | `08` | 自製字型 glyph 索引（13/16 点阵）→ UTF-8 |
| 音效 PCM | `.DIG` / `SAMPLE.*` | —（`04`/AIL） | 8-bit PCM → WAV |
| 地形控制表 | `FDSHAP.DAT` 小资源（2N+1） | `01` §5 | 直接 dump → JSON（已有 `docs/data/exe_tables/terrain.json` 形态） |
| 每屏调色板 | 各容器可能自带 | `01` §3 | 目前统一用 `FDOTHER#0`；后续按屏切 |

**两个图像 codec 的坑**：`knowledge-base` 记录的全幅图 RLE（`c>=0x80` 字面/`c<0x80` run）
与游戏里 `0x4E98D`（`rle_decode`：`tok>>6` 三模式）**不是同一套**；本工具的全幅图走前者
（标题/背景已视觉验证），后者用于 `res_blit` 那条路径（`src/game/rle.c`，已对拍）。
导出其它资源前先确认它属于哪一套，别混用。

## 4. 为什么不做成 C 工具

解码器虽然在 C 里已经对拍过（`rle.c`/`sprite24.c`/…），但导出是**离线一次性**的事，
用 Python 写零构建、零依赖（`zlib` 是标准库）、改起来快；等 §3 的 codec 都补齐、
需要大批量或需要与游戏解码逐字节对齐时，再把关键解码器抽成 C 工具复用也不迟
（`docs/TRANSLATION.md` §6 阶段 C 时本来就要把解码器留在 C 侧）。
