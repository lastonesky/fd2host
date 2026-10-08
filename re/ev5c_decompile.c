
// ===== 205B4 sz=10 =====
int __fastcall sub_205B4(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 4);
  return sub_205BE(v4, v5, a3, v6);
}


// ===== 335A0 sz=10 =====
void sub_335A0()
{
  JUMPOUT(0x33470);
}


// ===== 33674 sz=10 =====
void sub_33674()
{
  JUMPOUT(0x33470);
}


// ===== 34FC2 sz=10 =====
void sub_34FC2()
{
  JUMPOUT(0x34C57);
}


// ===== 350BE sz=10 =====
void sub_350BE()
{
  JUMPOUT(0x34F3D);
}


// ===== 34B07 sz=40 =====
void __fastcall sub_34B07(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 40);
  JUMPOUT(0x34885);
}


// ===== 34C52 sz=40 =====
void __fastcall sub_34C52(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 40);
  JUMPOUT(0x34F65);
}


// ===== 34940 sz=68 =====
void __fastcall sub_34940(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  int v7; // eax
  int v8; // edx
  int v9; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 40);
  v7 = sub_344F2(v4, v5, a3, v6, 37, 40, 0);
  sub_344F2(v7, v8, a3, v9, 13, 24, 0);
  JUMPOUT(0x34885);
}


// ===== 34A1E sz=78 =====
void __usercall sub_34A1E(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // eax
  int v9; // edx
  int v10; // ecx
  __int32 v11; // eax
  int v12; // edx
  int v13; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  v8 = sub_344F2(v5, v6, a4, v7, 48, 51, 7);
  sub_15F84(v8, v9, v10, a4, a5, dword_53A79, 6, 655360, 320, 205, 76, 74, 19, 1);
  sub_1366A(v11, v12, a4, v13, 24);
  JUMPOUT(0x34750);
}


// ===== 1D4F6 sz=39 =====
void __fastcall sub_1D4F6(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 16);
  sub_25A96(v4, v5, a3, v6, dword_53B13, -1, 1);
  JUMPOUT(0x1A80A);
}


// ===== 25464 sz=40 =====
void __fastcall sub_25464(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 40);
  JUMPOUT(0x231DF);
}


// ===== 2860A sz=40 =====
int __fastcall sub_2860A(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  sub_3702F(a1, a2, a3, a4, 4);
  if ( a5 == a6 )
    return 31;
  if ( a5 >= a6 )
    return 119;
  return 42;
}


// ===== 146A7 sz=42 =====
_BYTE *__fastcall sub_146A7(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  _BYTE *result; // eax

  sub_3702F(a1, a2, a3, a4, 4);
  result = (_BYTE *)(4 * (a5 + dword_53AC1 * a6) + dword_53A51 + 6);
  *result |= 0x80u;
  return result;
}


// ===== 13460 sz=40 =====
int __fastcall sub_13460(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 4);
  while ( MEMORY[0x46C] == dword_53A0C )
    ;
  dword_53A0C = MEMORY[0x46C];
  return MEMORY[0x46C];
}


// ===== 13536 sz=47 =====
int __fastcall sub_13536(__int32 a1, int a2, int a3, int a4)
{
  __int64 i; // rax

  for ( i = (unsigned int)sub_3702F(a1, a2, a3, a4, 8); SHIDWORD(i) < dword_53BEB; ++HIDWORD(i) )
  {
    LODWORD(i) = dword_53A45;
    *(_BYTE *)(80 * HIDWORD(i) + dword_53A45 + 5) &= ~0x80u;
  }
  return i;
}


// ===== 1D4CB sz=43 =====
_BYTE *__fastcall sub_1D4CB(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  _BYTE *result; // eax

  v4 = sub_3702F(a1, a2, a3, a4, 16);
  dword_53B13 = 0;
  result = sub_111BA(v4, v5, a3, v6, (int)aFdotherDat, 0, 80);
  dword_53B13 = (int)result;
  return result;
}


// ===== 2B749 sz=46 =====
void __fastcall sub_2B749(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // ebx
  int i; // eax

  sub_3702F(a1, a2, a3, a4, 8);
  v5 = 0;
  for ( i = 0; i < dword_53BFB - 1; ++i )
  {
    if ( *(_BYTE *)(a5 + i) != 0 )
      ++v5;
  }
  JUMPOUT(0x26ED6);
}


// ===== 173E7 sz=53 =====
int __fastcall sub_173E7(__int32 a1, int a2, int a3, int a4, int a5)
{
  int result; // eax

  result = sub_3702F(a1, a2, a3, a4, 4);
  for ( dword_53C57 = 0; dword_53C57 < 4; ++dword_53C57 )
  {
    result = dword_53C57;
    if ( *(_DWORD *)(a5 + 4 * dword_53C57) == 0 )
      break;
  }
  return result;
}


// ===== 24B14 sz=57 =====
int __fastcall sub_24B14(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int i; // ebx

  v5 = sub_3702F(a1, a2, a3, a4, 20);
  for ( i = 0; i < 16; ++i )
  {
    v5 = sub_2AEDB(v5, v6, i, v7, i, a5);
    if ( v5 != -1 )
      return 1;
  }
  return -1;
}


// ===== 25052 sz=55 =====
int __fastcall sub_25052(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  int result; // eax

  result = sub_3702F(a1, a2, a3, a4, 24);
  while ( a5 >= 0 )
  {
    sub_11DF2(0, 255, a5);
    result = j___delay(a6);
    --a5;
  }
  return result;
}


// ===== 344B4 sz=62 =====
char __fastcall sub_344B4(__int32 a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
  sub_3702F(a1, a2, a3, a4, 20);
  return sub_4E22A((char *)(*(_DWORD *)(a5 + 4 * a6 + 6) + a5), (char *)(a9 + a7 + a8 * a10), a8);
}


// ===== 25089 sz=67 =====
int __fastcall sub_25089(__int32 a1, int a2, int a3, int a4)
{
  unsigned __int8 i; // bl
  int v5; // eax
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 8);
  for ( i = 0; ; ++i )
  {
    result = i;
    if ( i >= dword_53BFB )
      break;
    v5 = 80 * i + dword_53BF7;
    *(_BYTE *)(v5 + 5) = 0;
    *(_WORD *)(v5 + 64) = *(_WORD *)(v5 + 66);
    *(_WORD *)(v5 + 68) = *(_WORD *)(v5 + 70);
  }
  return result;
}


// ===== 314DE sz=75 =====
_BYTE *__fastcall sub_314DE(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // ecx
  int v6; // edx
  __int32 v8; // [esp-2h] [ebp-14h]
  _BYTE v9[6]; // [esp+2h] [ebp-10h] BYREF

  sub_3702F(a1, a2, a3, a4, 32);
  v6 = 0;
  qmemcpy(v9, "012345", sizeof(v9));
  if ( a5 != 0 && *(_BYTE *)(a5 + 4) != 0 )
  {
    v8 = (unsigned __int8)v9[*(unsigned __int8 *)(a5 + 4) - 1];
    return sub_111BA(v8, 0, a3, v5, (int)aFdotherDat, 0, v8);
  }
  return (_BYTE *)v6;
}


// ===== 1E5C0 sz=81 =====
__int16 __fastcall sub_1E5C0(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // esi

  v5 = sub_3702F(a1, a2, a3, a4, 16);
  v8 = MEMORY[0x46C];
  do
  {
    sub_4E31C();
    LOBYTE(v6) = sub_10620(v5, v6, a3, v7);
    if ( MEMORY[0x46C] - v8 >= a5 || MEMORY[0x46C] < v8 )
      LOBYTE(v6) = 1;
    v5 = (unsigned __int8)v6;
  }
  while ( (_BYTE)v6 == 0 );
  return sub_4E381();
}


// ===== 1C220 sz=73 =====
int __fastcall sub_1C220(__int32 a1, int a2, int a3, int a4, char a5)
{
  int result; // eax
  int i; // ebx

  result = sub_3702F(a1, a2, a3, a4, 20);
  for ( i = 0; i < dword_53BEB; ++i )
  {
    result = *(unsigned __int8 *)(dword_53A45 + 80 * i + 6);
    if ( result == 2 )
    {
      result = sub_1BB8C(i, a5);
      if ( result != -1 )
        break;
    }
  }
  return result;
}


// ===== 26C9B sz=73 =====
int __fastcall sub_26C9B(__int32 a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
  int result; // eax
  int v9; // ebx
  int i; // esi

  result = sub_3702F(a1, a2, a3, a4, 32);
  v9 = 6 * a7 + *(_DWORD *)(dword_53F66 + 14) + dword_53F66 + 4;
  for ( i = 0; i < 9; ++i )
  {
    result = memmove(a5, v9, 6);
    a5 += a6;
    v9 += 6;
  }
  return result;
}


// ===== 34317 sz=79 =====
int __fastcall sub_34317(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  char v6; // cl
  unsigned __int8 i; // dl
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 12);
  v6 = 0;
  if ( a5 % 8 > 3 )
    v6 = 2;
  for ( i = 0; ; ++i )
  {
    result = i;
    if ( i >= 6u )
      break;
    *(_BYTE *)(a6 + i) = i + v6 + (a5 & 0xF8);
  }
  return result;
}


// ===== 1F6EF sz=80 =====
int __fastcall sub_1F6EF(__int32 a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
  int v8; // ebx
  int i; // esi
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 32);
  v8 = a5 + 320 * a6 + 655360;
  for ( i = 0; ; ++i )
  {
    result = a8 - 1;
    if ( i >= a8 - 1 )
      break;
    memset(v8, a7, a8 - 1);
    v8 += 320;
  }
  return result;
}


// ===== 2E95B sz=77 =====
int __fastcall sub_2E95B(__int32 a1, int a2, int a3, int a4)
{
  int v4; // ebx

  sub_3702F(a1, a2, a3, a4, 24);
  v4 = fopen(aFd2Tmp_1, &unk_502BA);
  dword_53A61 = malloc((char *)&loc_329FE + 2);
  sub_373CA((_BYTE *)dword_53A61, 1u, (int)&loc_329FE + 2, v4);
  return fclose(v4);
}


// ===== 1B5F1 sz=98 =====
int __fastcall sub_1B5F1(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // ecx
  int v6; // esi
  int i; // ebx
  unsigned __int8 *v8; // eax
  int v9; // edx
  __int32 v10; // eax

  sub_3702F(a1, a2, a3, a4, 20);
  v6 = 0;
  for ( i = 0; i < dword_53BEB; ++i )
  {
    v8 = (unsigned __int8 *)(80 * i + dword_53A45);
    if ( v8[6] == a5 )
    {
      v9 = v8[7];
      if ( v9 != 121 )
      {
        v10 = v8[31];
        if ( v10 != 10 && sub_34894(v10, v9, i, v5, i) == 0 )
          ++v6;
      }
    }
  }
  return v6;
}


// ===== 14B16 sz=98 =====
int __fastcall sub_14B16(__int32 a1, int a2, int a3, int a4, _BYTE *a5)
{
  int v6; // esi
  unsigned __int8 *v7; // edx
  int j; // ebx
  int i; // [esp+0h] [ebp-Ch]

  sub_3702F(a1, a2, a3, a4, 16);
  v6 = 0;
  v7 = (unsigned __int8 *)(dword_53A51 + 7);
  for ( i = 0; i < dword_53AC5; ++i )
  {
    for ( j = 0; j < dword_53AC1; ++j )
    {
      if ( *v7 != 255 )
      {
        *a5 = j;
        a5[1] = i;
        a5 += 2;
        ++v6;
      }
      v7 += 4;
    }
  }
  return v6;
}


// ===== 1B653 sz=100 =====
void __fastcall sub_1B653(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // esi
  int i; // ebx
  int v7; // eax

  sub_3702F(a1, a2, a3, a4, 28);
  v5 = 0;
  for ( i = 0; i < dword_53BEB; ++i )
  {
    v7 = dword_53A45 + 80 * i;
    if ( (*(_BYTE *)(v7 + 5) & 1) == 0 && *(_BYTE *)(v7 + 49) == 3 && *(_WORD *)(v7 + 64) == 0 )
      memmove(a5 + 3 * v5++, v7 + 49, 3);
  }
  JUMPOUT(0x1B64D);
}


// ===== 203BD sz=100 =====
int __fastcall sub_203BD(__int32 a1, int a2, int a3, int a4, char a5, char a6, char a7)
{
  int result; // eax
  int i; // ebx

  result = sub_3702F(a1, a2, a3, a4, 28);
  for ( i = 0; i < 256; ++i )
  {
    outp(968, i);
    outp(969, a5);
    outp(969, a6);
    result = outp(969, a7);
  }
  return result;
}


// ===== 208CF sz=87 =====
int __fastcall sub_208CF(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  int v7; // eax
  int v8; // edx
  int v9; // ecx
  int result; // eax

  v4 = sub_3702F(a1, a2, a3, a4, 8);
  v7 = sub_34894(v4, v5, a3, v6, 0);
  if ( v7 != 0 || (v7 = sub_34894(0, v8, a3, v9, 16)) != 0 || (v7 = sub_34894(0, v8, a3, v9, 17)) != 0 )
    dword_53ECC = 1;
  result = sub_34894(v7, v8, a3, v9, 52);
  if ( result != 0 )
    dword_53ECC = 2;
  return result;
}


// ===== 20AAF sz=101 =====
int __fastcall sub_20AAF(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  int v7; // eax
  int v8; // edx
  int v9; // ecx
  int result; // eax

  v4 = sub_3702F(a1, a2, a3, a4, 8);
  v7 = sub_34894(v4, v5, a3, v6, 0);
  if ( v7 != 0
    || (v7 = sub_34894(0, v8, a3, v9, 1)) != 0
    || (v7 = sub_34894(0, v8, a3, v9, 16)) != 0
    || (v7 = sub_34894(0, v8, a3, v9, 17)) != 0 )
  {
    dword_53ECC = 1;
  }
  result = sub_34894(v7, v8, a3, v9, 18);
  if ( result != 0 )
    dword_53ECC = 2;
  return result;
}


// ===== 20BF5 sz=122 =====
void __usercall sub_20BF5(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // eax
  int v9; // edx
  int v10; // ecx
  int v11; // eax
  int v12; // edx
  int v13; // ecx
  int v14; // eax
  int v15; // edx
  int v16; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  v8 = sub_34894(v5, v6, a4, v7, 20);
  if ( v8 != 0 )
    dword_53ECC = 2;
  v11 = sub_34894(v8, v9, a4, v10, 0);
  if ( v11 != 0 )
    dword_53ECC = 1;
  v14 = sub_34894(v11, v12, a4, v13, 1);
  if ( v14 != 0 )
  {
    sub_15F84(v14, v15, v16, a4, a5, dword_53A79, 7, 655360, 320, 205, 76, 74, 19, 1);
    dword_53ECC = 1;
  }
}


// ===== 20B72 sz=131 =====
void __usercall sub_20B72(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // edx
  int v6; // ecx
  int v7; // eax
  int v8; // edx
  int v9; // ecx
  int v10; // eax
  int v11; // edx
  int v12; // ecx

  sub_3702F(a1, a2, a4, a3, 40);
  if ( *(_BYTE *)(dword_53AD5 + 18) != 0 && *(_BYTE *)(dword_53AD5 + 19) != 0 && *(_BYTE *)(dword_53AD5 + 20) != 0 )
    dword_53ECC = 2;
  v7 = sub_34894(dword_53AD5, v5, a4, v6, 0);
  if ( v7 != 0 )
    dword_53ECC = 1;
  v10 = sub_34894(v7, v8, a4, v9, 1);
  if ( v10 != 0 )
  {
    sub_15F84(v10, v11, v12, a4, a5, dword_53A79, 9, 655360, 320, 205, 76, 74, 19, 1);
    dword_53ECC = 1;
  }
}


// ===== 1F04A sz=146 =====
unsigned int __fastcall sub_1F04A(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  unsigned __int8 *v6; // ebx
  unsigned __int8 *v7; // esi
  int v8; // edi
  unsigned int result; // eax

  sub_3702F(a1, a2, a3, a4, 20);
  v6 = (unsigned __int8 *)(dword_53A45 + 80 * a5);
  v7 = (unsigned __int8 *)(dword_53A45 + 80 * a6);
  v8 = abs(*v6 - *v7);
  if ( v8 <= abs(v6[1] - v7[1]) )
  {
    result = v7[1];
    if ( v6[1] <= result )
      v6[3] = 0;
    else
      v6[3] = 2;
  }
  else
  {
    result = *v7;
    if ( *v6 <= result )
      v6[3] = 3;
    else
      v6[3] = 1;
  }
  return result;
}


// ===== 1F0DC sz=167 =====
int __fastcall sub_1F0DC(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  unsigned __int8 *v6; // esi
  unsigned __int8 *v7; // ebx
  int result; // eax
  int v9; // edi
  int v10; // eax

  sub_3702F(a1, a2, a3, a4, 24);
  v6 = (unsigned __int8 *)(dword_53A45 + 80 * a5);
  v7 = (unsigned __int8 *)(dword_53A45 + 80 * a6);
  if ( v7[38] != 0 )
    return -1;
  v9 = abs(*v6 - *v7);
  if ( v9 + abs(v6[1] - v7[1]) != 1 )
    return -1;
  result = sub_1B83D(a6, 0);
  if ( result != -1 )
  {
    v10 = sub_1B722(a6, result);
    result = (unsigned __int8)sub_4E8BC(v10)[11];
    if ( result != 1 )
      return -1;
  }
  return result;
}


// ===== 205BE sz=28 =====
int __fastcall sub_205BE(__int32 a1, int a2, int a3, int a4)
{
  int i; // edx
  int v5; // eax
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 8);
  dword_53ECC = 2;
  for ( i = 0; i < dword_53BEB; ++i )
  {
    v5 = dword_53A45 + 80 * i;
    if ( *(_BYTE *)(v5 + 6) == 0 && (*(_BYTE *)(v5 + 5) & 1) == 0 )
      dword_53ECC = *(unsigned __int8 *)(v5 + 6);
  }
  result = dword_53A45;
  if ( (*(_BYTE *)(dword_53A45 + 5) & 1) != 0 )
    dword_53ECC = 1;
  return result;
}


// ===== 12CEA sz=145 =====
void __usercall sub_12CEA(
        __int32 a1@<eax>,
        int a2@<edx>,
        int a3@<ecx>,
        int a4@<ebx>,
        int a5@<edi>,
        int a6@<esi>,
        int a7,
        int a8)
{
  int v8; // eax
  int v9; // [esp-Ch] [ebp-Ch]
  int v10; // [esp-8h] [ebp-8h]
  int v11; // [esp-4h] [ebp-4h]

  sub_3702F(a1, a2, a4, a3, 20);
  v11 = a4;
  v10 = a6;
  v9 = a5;
  v8 = sub_11CAC(0);
  while ( a7 != (_DWORD)qword_53AB1 )
  {
    if ( a7 >= (int)qword_53AB1 )
      sub_11BFA(v9, v10, v11);
    else
      sub_11C59(v8);
    if ( dword_51A83 != 0 && dword_51A83 != 6 )
      sub_17AA9(1);
    LOWORD(v8) = sub_4E381();
  }
  while ( a8 != HIDWORD(qword_53AB1) )
  {
    if ( a8 >= SHIDWORD(qword_53AB1) )
      sub_11B9B(v9, v10, v11);
    else
      sub_11B48(v8);
    if ( dword_51A83 != 0 && dword_51A83 != 6 )
      sub_17AA9(1);
    LOWORD(v8) = sub_4E381();
  }
  JUMPOUT(0x12CE6);
}
