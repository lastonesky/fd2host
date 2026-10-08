/* ev6.c - FD2 scene-script handlers of funcs_1199C, first batch.
 * See ev6.h; written from the original disassembly (cdecl, last push = first
 * argument).  Services stay at their original addresses, exactly as ev2..ev5.
 */
#include "ev6.h"

#include <stdint.h>

/* --- services (original addresses) ------------------------------------- */
typedef void (*vm_fn)(void *stream, int sub, int addr, int pitch, int fg,
                      int shadow, int bgfill, int line_step, int wait);
typedef void (*i1_fn)(int);
typedef void (*i2_fn)(int, int);
typedef void (*i3_fn)(int, int, int);
typedef void (*v0_fn)(void);
typedef int  (*i1i_fn)(int);
typedef int  (*delay_fn)(int);

#define ORIG_VM_RUN   ((vm_fn)   (uintptr_t)0x00015F84u)
#define ORIG_135DD    ((i2_fn)   (uintptr_t)0x000135DDu)
#define ORIG_1366A    ((i1_fn)   (uintptr_t)0x0001366Au)
#define ORIG_11CAC    ((i1_fn)   (uintptr_t)0x00011CACu)
#define ORIG_134E4    ((v0_fn)   (uintptr_t)0x000134E4u)
#define ORIG_FLUSH    ((v0_fn)   (uintptr_t)0x0004E381u)
#define ORIG_344F2    ((i3_fn)   (uintptr_t)0x000344F2u)
#define ORIG_34894    ((i1i_fn)  (uintptr_t)0x00034894u)
#define ORIG_32975    ((i1_fn)   (uintptr_t)0x00032975u)
#define ORIG_UNIT_ADD ((i1i_fn)  (uintptr_t)0x000112A5u)
#define ORIG_LOAD     ((i1i_fn)  (uintptr_t)0x00010B4Eu)
#define ORIG_MAP      ((i1_fn)   (uintptr_t)0x00032999u)
#define ORIG_DELAY    ((delay_fn)(uintptr_t)0x0003790Au)

/* --- globals ----------------------------------------------------------- */
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)
#define dword_53A55 (*(uint32_t *)(uintptr_t)0x00053A55u)
#define dword_53A79 (*(uint32_t *)(uintptr_t)0x00053A79u)
#define dword_53AD5 (*(uint32_t *)(uintptr_t)0x00053AD5u)
#define dword_53BEF (*(uint32_t *)(uintptr_t)0x00053BEFu)
#define dword_53EC8 (*(uint32_t *)(uintptr_t)0x00053EC8u)
#define dword_51A83 (*(uint32_t *)(uintptr_t)0x00051A83u)
#define byte_53AFA  (*(uint8_t  *)(uintptr_t)0x00053AFAu)

#define REC 0x50

/* Tail of every handler: vm_run(stream, sub, 0xA0000, 320, 205, 76, 74, 19, 1). */
static void vm_play(int sub)
{
    ORIG_VM_RUN((void *)(uintptr_t)dword_53A79, sub, 0x000A0000, 320,
                205, 76, 74, 19, 1);
}

#define R(i) (dword_53A45 + REC * (uint32_t)(i))

/* 0x34531 - idx[0]: two sprite loads with a vm_run between. */
void ev6_34531(int arg)
{
    (void)arg;
    ORIG_UNIT_ADD(1);
    ORIG_LOAD(3);
    ORIG_135DD(5, 8);
    ORIG_11CAC(1);
    ORIG_DELAY(100);
    ORIG_1366A(7);
    ORIG_FLUSH();
    vm_play(11);
    dword_51A83 = 0;
    ORIG_LOAD(7);
    ORIG_11CAC(1);
    ORIG_DELAY(100);
    ORIG_1366A(8);
    ORIG_FLUSH();
    vm_play(3);
    ORIG_134E4();
}

/* 0x3460B - idx[1]. */
void ev6_3460B(int arg)
{
    (void)arg;
    ORIG_135DD(11, 16);
    ORIG_MAP(4);
    ORIG_FLUSH();
    ORIG_11CAC(1);
    ORIG_1366A(3);
    ORIG_134E4();
    vm_play(4);
}

/* 0x34673 - idx[2]. */
void ev6_34673(int arg)
{
    (void)arg;
    ORIG_135DD(0, 16);
    ORIG_MAP(5);
    ORIG_FLUSH();
    ORIG_11CAC(1);
    ORIG_1366A(4);
    ORIG_134E4();
    vm_play(5);
}

/* 0x346CD - idx[3]. */
void ev6_346CD(int arg)
{
    (void)arg;
    ORIG_135DD(11, 11);
    byte_53AFA = 1;
    ORIG_LOAD(6);
    byte_53AFA = 0;
    ORIG_11CAC(1);
    ORIG_1366A(6);
    ORIG_134E4();
    ORIG_FLUSH();
    vm_play(6);
}

/* 0x34778 - idx[6]: then stamp two bytes into records 5..10. */
void ev6_34778(int arg)
{
    int i;
    (void)arg;
    ORIG_135DD(9, 1);
    ORIG_DELAY(100);
    byte_53AFA = 1;
    ORIG_LOAD(3);
    byte_53AFA = 0;
    ORIG_1366A(13);
    ORIG_DELAY(200);
    vm_play(4);
    for (i = 5; i < 11; i++) {
        *(uint8_t *)(uintptr_t)(R(i) + 53) = 26;
        *(uint8_t *)(uintptr_t)(R(i) + 54) = 15;
    }
}

/* 0x350BE - idx[5]: tail-merged with 0x34F38. */
void ev6_350BE(int arg)
{
    (void)arg;
    ORIG_LOAD(1);
    vm_play(1);
}

/* 0x350C8 - idx[7]. */
void ev6_350C8(int arg)
{
    (void)arg;
    ORIG_135DD(27, 5);
    byte_53AFA = 1;
    ORIG_LOAD(2);
    byte_53AFA = 0;
    ORIG_1366A(46);
    ORIG_134E4();
    vm_play(8);
}

/* 0x34818 - idx[9]: only when flag 6 is clear. */
void ev6_34818(int arg)
{
    (void)arg;
    if (ORIG_34894(6) != 0)
        return;
    ORIG_LOAD(2);
    ORIG_135DD(3, 0);
    ORIG_DELAY(800);
    ORIG_135DD(3, 17);
    ORIG_DELAY(200);
    vm_play(4);
}

/* 0x348BB - idx[11]. */
void ev6_348BB(int arg)
{
    (void)arg;
    ORIG_LOAD(2);
    vm_play(2);
}

/* 0x34940 - idx[14]. */
void ev6_34940(int arg)
{
    (void)arg;
    ORIG_344F2(37, 40, 0);
    ORIG_344F2(13, 24, 0);
    vm_play(3);
}

/* 0x34984 - idx[15]. */
void ev6_34984(int arg)
{
    (void)arg;
    dword_51A83 = 0;
    byte_53AFA = 1;
    ORIG_LOAD(2);
    byte_53AFA = 0;
    ORIG_135DD(14, 0);
    ORIG_1366A(23);
    ORIG_134E4();
    ORIG_344F2(7, 12, 0);
    ORIG_344F2(33, 35, 0);
    vm_play(4);
}

/* 0x349EC - idx[16]. */
void ev6_349EC(int arg)
{
    (void)arg;
    ORIG_LOAD(3);
    vm_play(5);
}

/* 0x34A1E - idx[17]: the tail at 0x34750 is a second vm_run, sub 7. */
void ev6_34A1E(int arg)
{
    (void)arg;
    ORIG_344F2(48, 51, 7);
    vm_play(6);
    ORIG_1366A(24);
    vm_play(7);
}

/* 0x34B07 - idx[20]: tail-merged with the vm_run(1) block. */
void ev6_34B07(int arg)
{
    (void)arg;
    vm_play(1);
}

/* 0x34B6F - idx[22]: tail-merged with the vm_run(3) body. */
void ev6_34B6F(int arg)
{
    (void)arg;
    if (ORIG_34894(8) != 0)
        return;
    ORIG_LOAD(1);
    vm_play(3);
}

/* 0x34B9A - idx[23]. */
void ev6_34B9A(int arg)
{
    (void)arg;
    ORIG_344F2(8, 28, 0);
    vm_play(4);
    if ((int)dword_53BEF >= 15)
        return;
    ORIG_LOAD(2);
    ORIG_135DD(5, 17);
    ORIG_1366A(25);
    vm_play(5);
    ORIG_135DD(5, 17);
    ORIG_1366A(26);
    ORIG_32975(33);
    dword_51A83 = 1;
}

/* 0x34C52 - idx[24]: bare vm_run(3); 0x34FC2 is byte-identical. */
void ev6_34C52(int arg)
{
    (void)arg;
    vm_play(3);
}

/* 0x34C7A - idx[25]: only when the status block's +16 byte is 1. */
void ev6_34C7A(int arg)
{
    (void)arg;
    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 16) != 1)
        return;
    byte_53AFA = 1;
    ORIG_LOAD(2);
    byte_53AFA = 0;
    ORIG_135DD(16, 10);
    ORIG_1366A(30);
    vm_play(2);
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 17) = 1;
}

/* 0x34D2F - idx[27]. */
void ev6_34D2F(int arg)
{
    (void)arg;
    ORIG_135DD(8, 2);
    ORIG_DELAY(100);
    ORIG_LOAD((int)dword_53BEF);
    ORIG_DELAY(100);
}

/* 0x34DD0 - idx[30]: clear records 12..33, seed record 11, two vm_runs. */
void ev6_34DD0(int arg)
{
    uint32_t v7;
    int i;
    (void)arg;
    for (i = 12; i < 34; i++)
        *(uint8_t *)(uintptr_t)(R(i) + 52) = 0;
    *(uint8_t *)(uintptr_t)(dword_53A55 + 3) = (uint8_t)(dword_53BEF + 1);
    *(uint8_t *)(uintptr_t)(dword_53A55 + 6) = (uint8_t)(dword_53BEF + 2);
    v7 = dword_53A45 + 880;
    *(uint8_t *)(uintptr_t)(v7 + 5) = 0;
    *(uint8_t *)(uintptr_t)(v7 + 6) = 1;
    *(uint8_t *)(uintptr_t)(v7 + 7) = 6;
    *(uint8_t *)(uintptr_t)(v7 + 8) = 6;
    *(uint8_t *)(uintptr_t)(v7 + 0x31) = 0xFF;
    *(uint8_t *)(uintptr_t)(v7 + 0x34) = 0x80;
    *(uint16_t *)(uintptr_t)(v7 + 0x40) = 1;
    vm_play(2);
    ORIG_LOAD(1);
    vm_play(3);
    dword_53EC8 = 0;
    *(uint8_t *)(uintptr_t)(dword_53AD5 + 16) = 2;
}

/* 0x34EB3 - idx[31]: four window moves around the current status value. */
void ev6_34EB3(int arg)
{
    (void)arg;
    ORIG_LOAD(*(uint8_t *)(uintptr_t)(dword_53AD5 + 16));
    ++*(uint8_t *)(uintptr_t)(dword_53AD5 + 16);
    ORIG_135DD(0, 0);
    ORIG_DELAY(200);
    ORIG_135DD(12, 0);
    ORIG_DELAY(200);
    ORIG_135DD(12, 11);
    ORIG_DELAY(200);
    ORIG_135DD(0, 11);
    ORIG_DELAY(200);
}

/* 0x34F38 - idx[32]: bare load + vm_run(1); 0x350BE is byte-identical. */
void ev6_34F38(int arg)
{
    (void)arg;
    ORIG_LOAD(1);
    vm_play(1);
}

/* 0x34FC2 - idx[34]: tail-merged with 0x34C52's vm_run(3) body. */
void ev6_34FC2(int arg)
{
    (void)arg;
    vm_play(3);
}

/* 0x34FCC - idx[35]. */
void ev6_34FCC(int arg)
{
    (void)arg;
    ORIG_135DD(12, 5);
    byte_53AFA = 1;
    ORIG_LOAD(2);
    byte_53AFA = 0;
    ORIG_1366A(42);
    ORIG_134E4();
}

/* 0x35022 - idx[37]. */
void ev6_35022(int arg)
{
    (void)arg;
    vm_play(1);
    ORIG_135DD(15, 34);
    byte_53AFA = 1;
    ORIG_LOAD(3);
    byte_53AFA = 0;
    ORIG_1366A(43);
    ORIG_134E4();
    ORIG_135DD(0, 26);
    byte_53AFA = 1;
    ORIG_LOAD(4);
    byte_53AFA = 0;
    ORIG_1366A(44);
    ORIG_134E4();
    dword_51A83 = 1;
}
