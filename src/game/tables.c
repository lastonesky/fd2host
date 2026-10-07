/* tables.c - generic index helpers for the obj0 table accessors.
 *
 * Translation of the eleven one-line accessors 0x4E7DD..0x4E8BC. Each is
 * base + stride*index + offset with a 32-bit unsigned multiply; 0x4E87D is
 * a dword lookup instead. See game/tables.h for the per-function constants.
 */
#include "tables.h"

void *tbl_ptr(void *base, int stride, int index, int offset)
{
    return (char *)base + (uint32_t)stride * (uint32_t)index + (uint32_t)offset;
}

uint32_t tbl_u32(const void *base, int index)
{
    return *(const uint32_t *)((const char *)base + 4u * (uint32_t)index);
}

/* 0x4EB48 - the pointer table at 0x627D8 (one entry per argument). */
void *tbl_off627D8(int i)
{
    return *(void **)(uintptr_t)(0x000627D8u + 4u * (uint32_t)i);
}
