# 第 47 轮：CRT 重定向对拍器 + 世界加载两件套（第 301–302 个）

> 按 `docs/TRANSLATION.md` §5 的“优先级 1”执行：先建 **CRT 重定向对拍器**，
> 再补 `menu_reload_world` 的两个前置——`0x10652`（地图图块）与 `0x1088D`（队伍/场景记录）。
> 接入 300 → **302 / 1359（22.2%）**。

## 47.1 新对拍器 `src/worldcheck.c`

`0x10652`/`0x1088D` 都经 Watcom CRT（`malloc/free/fopen/fclose/fseek/fread`）+ 真实数据文件。
harness 做三件事：

1. **CRT 重定向**：把 `0x3706E/0x3776E/0x37324/0x3759C/0x37940/0x373CA/0x377A3` 七个入口写成
   `jmp` 到宿主 libc（`rescheck`/`ev6check` 同款手术）。其中 **`stub_malloc` 会 memset 0**——
   原版确实会读“已分配但未写”的尾部，两侧必须从同一份零起点比较，这是 harness 性质，
   不是对游戏的断言。
2. **已接入 helper 走 C**：`0x111BA→res_load`、`0x4E98D→rle_decode`、`0x24D22→map_scroll_lines`、
   `0x4DF4C→util_fix_records`；`0x11019`/`0x1B750` 在本 harness **桩掉**（各自有 ev6check 专测，
   且 `unit_entry_data` 会按记录 `+7` 去 FDICON.B24 寻址，随机记录会让它越界——不是被测函数的问题）。
   `0x10B4E` 桩掉，避免写 FD2.TMP。
3. **真实文件**：从 `E:\FD2` 运行，`FDTXT/FDFIELD/FDSHAP/FDOTHER/FDICON` 直接读原件；
   跑 `world_load_tiles` 的 10 个视图模式（9/17/21/22/23/24/25/27/28/29）与
   `world_load_party` 的同样 10 个 slot，逐字节比对 **记录缓冲**、**图块缓冲** 与全部标量全局。

判据：`worldcheck --cases=60` = **60/0**。

## 47.2 新模块 `src/game/world_load.c/.h`（2 个）

| 地址 | 名字 | 作用 |
|---|---|---|
| `0x10652` | `world_load_tiles` | 按 `dword_53C03` 选图块集：`{9,24,25}/{28,29}` 只 load+malloc(64000)；`{17,21,22,27}` 两半 `rle_decode` 进 `h×w`；`23` 解码 312 宽 + `map_scroll_lines(0)` |
| `0x1088D` | `world_load_party` | 载入 `FDTXT#slot+1`/`FDFIELD#{3s,3s+1,3s+2}`/`FDSHAP`，重建 `dword_53BE7` 条 80 字节记录（列表源 `dword_53BF7`、`+7` 索引取 FDICON.B24 blob），最后 `unit_sprites_build(0)` |

接入分组 `REPL_MENU`（`0x205DA menu_reload_world` 的同一族）。

## 47.3 关键实现点

1. **`0x10652` 的两个 rle 目的地是同一块 `malloc(h*w)`**：第一半 `y=0`、第二半 `y=h/2`，
   写入区域覆盖整幅；`dword_53B03` 解压源用完即 `free` 置 0。
2. **`0x1088D` 的 `slot` 同时是 `dword_53C03`**：`menu_reload_world` 传的就是它，
   `world_load_tiles()`（C 名直接调用）与机器码 `0x10652` 走同一条逻辑。
3. **`fp` 不能强转成 `int` 传**：`unit_entry_data(int, void*)` 的第二个参数是 `void*`，
   `(int)(uintptr_t)fp` 在 gcc `-Wint-conversion` 下直接报错（Linux 0 warning 要求）。
4. **对拍器要按 case 限长拷贝**：`capture()` 一开始无条件 `memcpy 0x40000`，而资源模式的
   `dword_53AFF` 只有几百字节 → 段错误。改成按 `tile_len(case)`（资源模式取 `dword_53BFF`）。

## 47.4 判据

```
worldcheck --cases=60：60/0
ev2check --cases=15 全量：2400/0（未受影响）
regress：all 8/8          repl: installed 302 (mask 0x3FFFFF)
静态帧 A/B --shot-tick=500：0 / 64000 px
letest：exact match
Linux：make 0 warning、letest-linux exact match、doscheck-linux 49/49
translation_map：302 wired / 1359（22.2%），0 translated-not-wired
```

## 47.5 下一步

按优先级 1 的剩余项：用**同一把 CRT 重定向对拍器**继续收堆叶子
`0x15F0E`/`0x1DF58`/`0x1C2DA`/`0x1C4CC`/`0x24618`（+ `0x24618` 的依赖链 `0x22046`/`0x219AD`），
它们只需要再加一点屏幕缓冲/记录世界；然后按优先级 2 做**地图世界 harness**，
收 `0x197E5`/`0x19953`/`0x1DB65`/`0x14818`/`0x12CEA`。
