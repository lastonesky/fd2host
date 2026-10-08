/* entry.c - process entry point, shared by every entry layer.
 *
 * The CRT heap starts at 0x10000 and would take the window the DOS/4GW
 * objects must live in, so the address space has to be reserved before CRT
 * initialisation. Zero CRT usage is allowed here. Both main_win32.c and
 * main_sokol.c link this file and simply provide their own main(). */

#include <windows.h>
#include "le.h"

int __cdecl mainCRTStartup(void);

void __cdecl fd2_entry(void)
{
    le_reserve_address_space_early();
    ExitProcess((UINT)mainCRTStartup());
}
