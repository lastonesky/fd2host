/* probe4.c - what does Windows do with a software `int NN` executed from a
 * 32-bit user process?  If every vector raises a VEH-catchable exception with
 * EIP pointing at the `int`, the host can stop rewriting CD -> CC entirely
 * (the rewrite currently corrupts operands that merely contain CD 21-like
 * bytes, e.g. the displacement of `call sub_34894` at 0x127C2).
 */
#include <windows.h>
#include <stdio.h>

static volatile int g_in_test;
static volatile unsigned g_vec;

static LONG WINAPI veh(EXCEPTION_POINTERS *e)
{
    if (g_in_test) {
        printf("  vec %02X -> code %08lX  ExceptionAddress=%p  Eip=%08lX  Eip-ExceptionAddress=%d\n",
               (unsigned)g_vec,
               (unsigned long)e->ExceptionRecord->ExceptionCode,
               e->ExceptionRecord->ExceptionAddress,
               (unsigned)e->ContextRecord->Eip,
               (int)((long)e->ContextRecord->Eip - (long)(uintptr_t)e->ExceptionRecord->ExceptionAddress));
        fflush(stdout);
        g_in_test = 0;
        /* skip the instruction: int NN is 2 bytes, int3 (CC) is 1, int 03 (CD 03) is 2 */
        if (e->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT &&
            *(const unsigned char *)e->ExceptionRecord->ExceptionAddress == 0xCC)
            e->ContextRecord->Eip += 1;
        else
            e->ContextRecord->Eip += 2;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void test(unsigned vec)
{
    g_vec = vec;
    g_in_test = 1;
    __try {
        switch (vec) {
        case 0x03: __asm { int 3 }   break;
        case 0x08: __asm { int 8 }   break;
        case 0x09: __asm { int 9 }   break;
        case 0x10: __asm { int 0x10 } break;
        case 0x16: __asm { int 0x16 } break;
        case 0x1A: __asm { int 0x1A } break;
        case 0x20: __asm { int 0x20 } break;
        case 0x21: __asm { int 0x21 } break;
        case 0x28: __asm { int 0x28 } break;
        case 0x2F: __asm { int 0x2F } break;
        case 0x31: __asm { int 0x31 } break;
        case 0x33: __asm { int 0x33 } break;
        case 0x67: __asm { int 0x67 } break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("  vec %02X -> unhandled, code %08lX\n", (unsigned)vec,
               (unsigned long)GetExceptionCode());
        g_in_test = 0;
    }
    if (g_in_test) {
        printf("  vec %02X -> survived without exception\n", (unsigned)vec);
        g_in_test = 0;
    }
}

int main(void)
{
    unsigned vecs[] = { 0x03, 0x08, 0x09, 0x10, 0x16, 0x1A, 0x20, 0x21,
                        0x28, 0x2F, 0x31, 0x33, 0x67 };
    unsigned i;

    AddVectoredExceptionHandler(1, veh);
    printf("software int NN from ring 3:\n");
    for (i = 0; i < sizeof vecs / sizeof vecs[0]; i++) {
        printf("int %02X:\n", vecs[i]);
        fflush(stdout);
        test(vecs[i]);
    }
    printf("done\n");
    return 0;
}
