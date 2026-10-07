/* kbd.h - BIOS keyboard-buffer helpers (main state machine family).
 *
 *   0x4E381  kbd_flush()      drop pending keys: BDA 0x41C = BDA 0x41A
 *   0x10620  kbd_pending()    1 while the buffer holds a key (0x41C != 0x41A)
 *
 * The machine code touches the BDA directly. In the host the BDA lives in the
 * 64 KiB low-memory mirror (dos.c maps it at DOS_LOWMEM_BASE, normally 0x70000)
 * and the game's own absolute references are redirected there by
 * dos_patch_lowmem_refs(); the C reads the same mirror.
 */
#ifndef GAME_KBD_H
#define GAME_KBD_H

void kbd_flush(void);
int  kbd_pending(void);

#endif /* GAME_KBD_H */
