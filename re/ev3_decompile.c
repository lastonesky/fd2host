
// ===== 352CA =====
void __fastcall sub_352CA(__int32 a1, int a2, int a3, int a4, unsigned __int8 *a5)
{
  int v5; // edx
  int v6; // ecx
  __int32 v7; // eax
  int v8; // edx
  int v9; // ecx
  _BYTE v10[3]; // [esp-Ch] [ebp-Ch] BYREF
  unsigned __int8 v11; // [esp-9h] [ebp-9h] BYREF

  sub_3702F(a1, a2, a3, a4, 52);
  *(_WORD *)v10 = unk_52742;
  v10[2] = unk_52744;
  sub_1AA1D((__int32)v10, v5, a3, v6, a5, 1, (int)v10);
  sub_15F84(v7, v8, v9, a3, &v11, dword_53A79, 11, 655360, 320, 205, 76, 74, 19, 1);
}


// ===== 35346 =====
void __fastcall sub_35346(__int32 a1, int a2, int a3, int a4, unsigned __int8 *a5)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // edx
  int v9; // ecx
  _DWORD v10[3]; // [esp-Ch] [ebp-Ch] BYREF

  v5 = sub_3702F(a1, a2, a3, a4, 52);
  LOWORD(v10[0]) = unk_52745;
  BYTE2(v10[0]) = unk_52747;
  sub_15F84(v5, v6, v7, a3, (unsigned __int8 *)v10 + 3, dword_53A79, 3, 655360, 320, 205, 76, 74, 19, 1);
  sub_1AA1D((__int32)v10, v8, a3, v9, a5, 1, (int)v10);
  v10[2] = 1;
  v10[1] = 19;
  v10[0] = 74;
  JUMPOUT(0x3530D);
}


// ===== 35468 =====
void __usercall sub_35468(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // ecx
  int v6; // eax
  int v7; // edx
  int v8; // ecx
  int v9; // eax
  int v10; // edx
  int v11; // ecx
  __int32 v12; // eax
  int v13; // edx
  int v14; // ecx
  int v15; // eax
  int v16; // edx
  int v17; // ecx
  __int32 v18; // eax
  int v19; // edx
  int v20; // ecx
  int v21; // eax
  int v22; // edx
  int v23; // ecx
  __int32 v24; // eax
  int v25; // edx
  int v26; // ecx
  int v27; // eax
  int v28; // edx
  int v29; // ecx
  __int32 v30; // eax
  int v31; // edx
  int v32; // ecx
  int v33; // eax
  int v34; // edx
  int v35; // ecx

  sub_3702F(a1, a2, a4, a3, 40);
  v6 = sub_10B4E(dword_53BEF / 2, dword_53BEF >> 31, a4, v5, dword_53BEF / 2);
  v9 = sub_17AA9(v6, v7, a4, v8, 1);
  sub_135DD(v9, v10, a4, v11, 0, 0);
  v15 = sub_17AA9(v12, v13, a4, v14, 8);
  sub_135DD(v15, v16, a4, v17, 28, 0);
  v21 = sub_17AA9(v18, v19, a4, v20, 8);
  sub_135DD(v21, v22, a4, v23, 28, 32);
  v27 = sub_17AA9(v24, v25, a4, v26, 8);
  sub_135DD(v27, v28, a4, v29, 0, 32);
  v33 = sub_17AA9(v30, v31, a4, v32, 8);
  if ( dword_53BEF == 2 )
    sub_15F84(v33, v34, v35, a4, a5, dword_53A79, 3, 655360, 320, 205, 76, 74, 19, 1);
}


// ===== 355F0 =====
void __fastcall sub_355F0(__int32 a1, int a2, int a3, int a4, unsigned __int8 *a5)
{
  int v5; // edx
  int v6; // ecx
  _DWORD v7[3]; // [esp-Ch] [ebp-Ch] BYREF

  sub_3702F(a1, a2, a3, a4, 52);
  LOWORD(v7[0]) = unk_52748;
  BYTE2(v7[0]) = unk_5274A;
  sub_1AA1D((__int32)v7, v5, a3, v6, a5, 1, (int)v7);
  v7[2] = 1;
  v7[1] = 19;
  v7[0] = 74;
  JUMPOUT(0x3530D);
}


// ===== 356B3 =====
int __fastcall sub_356B3(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  int v7; // eax
  int v8; // edx
  int v9; // ecx
  __int32 v10; // eax
  int v11; // edx
  int v12; // ecx
  __int32 v13; // eax
  int v14; // edx
  int v15; // ecx
  __int32 v16; // eax
  int v17; // edx
  int v18; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 12);
  v7 = sub_10B4E(v4, v5, a3, v6, dword_53BEF);
  sub_135DD(v7, v8, a3, v9, 0, 4);
  v10 = j___delay(400);
  sub_135DD(v10, v11, a3, v12, 0, 22);
  v13 = j___delay(400);
  sub_135DD(v13, v14, a3, v15, 26, 24);
  v16 = j___delay(400);
  sub_135DD(v16, v17, a3, v18, 26, 2);
  return j___delay(400);
}


// ===== 35730 =====
void __fastcall sub_35730(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // edx
  int v6; // ecx
  int v7; // eax
  __int64 v8; // rax
  int v9; // ecx
  int v10; // edx
  int v11; // ecx
  __int32 v12; // eax
  __int32 v13; // eax
  int v14; // edx
  int v15; // ecx
  int v16; // edx
  int v17; // ecx
  _BYTE v18[3]; // [esp-Ch] [ebp-Ch] BYREF
  unsigned __int8 v19; // [esp-9h] [ebp-9h] BYREF

  sub_3702F(a1, a2, a3, a4, 52);
  *(_WORD *)v18 = unk_5274B;
  v18[2] = unk_5274D;
  if ( a5 == 0 && *(_BYTE *)dword_53AD5 == 0 )
  {
    sub_15F84(dword_53AD5, v5, v6, a3, &v19, dword_53A79, 0, 655360, 320, 205, 76, 74, 19, 1);
    v7 = sub_2E2B0(17, 0);
    v8 = sub_1DB65(v7);
    if ( sub_34894(v8, SHIDWORD(v8), a3, v9, 17) != 0 )
    {
      v12 = dword_53AD5;
      *(_BYTE *)dword_53AD5 = 1;
      LOBYTE(v13) = sub_12263(v12, v10, a3, v11);
      sub_11CAC(v13, v14, a3, v15, 1);
      sub_1AA1D((__int32)v18, v16, a3, v17, nullptr, 1, (int)v18);
    }
  }
  dword_53EC8 = 0;
  JUMPOUT(0x3531B);
}


// ===== 357DD =====
int __usercall sub_357DD@<eax>(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  __int32 v8; // eax
  int v9; // edx
  int v10; // ecx
  int v11; // eax
  int v12; // edx
  int v13; // ecx
  int v14; // eax
  int v15; // edx
  int v16; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  sub_135DD(v5, v6, a4, v7, 6, 40);
  v11 = sub_10B4E(v8, v9, a4, v10, 1);
  v14 = sub_1366A(v11, v12, a4, v13, 74);
  sub_15F84(v14, v15, v16, a4, a5, dword_53A79, 5, 655360, 320, 205, 76, 74, 19, 1);
  return sub_134E4();
}


// ===== 35833 =====
void __fastcall sub_35833(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 12);
  sub_10B4E(v4, v5, a3, v6, dword_53BEF);
  JUMPOUT(0x3571A);
}


// ===== 35854 =====
char __fastcall sub_35854(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // ecx
  int v6; // edx
  __int32 v7; // eax
  int v8; // edx
  int v9; // ecx
  __int16 *v10; // eax
  int v11; // edx
  int v12; // ecx
  int v13; // eax
  __int32 v15; // eax
  int v16; // edx
  int v17; // ecx
  __int16 *v18; // eax
  int v19; // edx
  int v20; // ecx
  int v21; // eax
  int v22; // edx
  int v23; // ecx
  int v24; // [esp-2h] [ebp-1Ch] BYREF
  int v25; // [esp+6h] [ebp-14h]
  char v26; // [esp+Ah] [ebp-10h]
  unsigned __int8 v27[15]; // [esp+Bh] [ebp-Fh] BYREF

  sub_3702F(a1, a2, a3, a4, 68);
  *(_DWORD *)&v27[11] = a3;
  v25 = unk_5274E;
  v26 = unk_52752;
  sub_4E381();
  v6 = 80 * a5;
  sub_1956B(*(unsigned __int8 *)(80 * a5 + dword_53A45 + 7));
  if ( sub_1B8A6(a5) == 8 )
  {
    sub_15F84(8, v6, v5, a3, v27, dword_53A7D, 480, 696099, 320, 205, 76, 74, 19, 1);
    v10 = sub_16559(v7, v8, a3, v9, 0);
    sub_16C57((__int32)v10, v11, a3, v12, 0);
    return sub_196CB(v13);
  }
  else
  {
    sub_12E38(qword_53AB1, HIDWORD(qword_53AB1), &v24);
    LOBYTE(a3) = BYTE2(v24);
    dword_53AD9 = *((unsigned __int8 *)&v25 + BYTE2(v24)) + 181;
    sub_15F84(dword_53AD9, v6, v5, a3, v27, dword_53A7D, 422, 696099, 320, 205, 76, 74, 19, 1);
    v18 = sub_16559(v15, v16, a3, v17, 0);
    sub_16C57((__int32)v18, v19, a3, v20, 0);
    v21 = sub_1BB8C(a5, *((_BYTE *)&v25 + (unsigned __int8)a3));
    sub_196CB(v21);
    LOBYTE(a3) = 0;
    while ( (unsigned __int8)a3 < 5u )
    {
      v22 = dword_53AD5;
      *(_BYTE *)(dword_53AD5 + (unsigned __int8)a3) = 1;
      LOBYTE(a3) = a3 + 1;
    }
    return sub_12263((unsigned __int8)a3, v22, a3, v23);
  }
}


// ===== 35A0D =====
void __usercall sub_35A0D(
        __int32 a1@<eax>,
        int a2@<edx>,
        int a3@<ecx>,
        int a4@<ebx>,
        unsigned __int8 *a5@<edi>,
        int a6)
{
  int v6; // edx
  int v7; // ecx
  int v8; // ebx
  int v9; // eax
  __int32 v10; // eax
  int v11; // edx
  int v12; // ecx
  __int16 *v13; // eax
  int v14; // edx
  int v15; // ecx
  int v16; // eax
  __int32 v17; // eax
  __int32 v18; // eax
  int v19; // edx
  int v20; // ecx
  int v21; // eax
  __int32 v22; // eax
  int v23; // edx
  int v24; // ecx
  int v25; // edx
  int v26; // ecx
  _BYTE *v27; // esi
  int i; // ebx
  __int32 v29; // eax
  __int32 v30; // eax
  __int32 v31; // eax
  int v32; // edx
  int v33; // ecx
  __int32 v34; // eax
  int v35; // edx
  int v36; // ecx

  sub_3702F(a1, a2, a4, a3, 48);
  if ( *(_BYTE *)(dword_53AD5 + 12) == 0 )
  {
    v8 = 80 * a6;
    sub_1956B(*(unsigned __int8 *)(80 * a6 + dword_53A45 + 7));
    v9 = sub_2AEDB(a6, 208);
    if ( v9 == -1 )
    {
      sub_15F84(-1, v6, v7, v8, a5, dword_53A79, 2, 693535, 320, 205, 76, 74, 19, 1);
      v13 = sub_16559(v10, v11, v8, v12, 0);
      sub_16C57((__int32)v13, v14, v8, v15, 0);
      sub_196CB(v16);
    }
    else
    {
      v17 = sub_1B8E7(a6, v9);
      sub_15F84(v17, v6, v7, v8, a5, dword_53A79, 3, 693535, 320, 205, 76, 74, 19, 1);
      sub_16C57(v18, v19, v8, v20, 0);
      v22 = sub_196CB(v21);
      v27 = sub_111BA(v22, v23, v8, v24, (int)aFdotherDat, 0, 45);
      for ( i = 0; i < 59; ++i )
      {
        v29 = sub_2EB9F(v27, i, 703716, 320, -1);
        sub_17AA9(v29, v25, i, v26, 2);
      }
      free(v27);
      v30 = dword_53AD5;
      *(_BYTE *)(dword_53AD5 + 12) = 1;
      LOBYTE(v31) = sub_12263(v30, v25, i, v26);
      sub_10B4E(v31, v32, i, v33, 1);
      v34 = sub_112A5(31);
      sub_15F84(v34, v35, v36, i, a5, dword_53A79, 4, 655360, 320, 205, 76, 74, 19, 1);
    }
  }
}


// ===== 35C40 =====
int __usercall sub_35C40@<eax>(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // edx
  int v6; // ecx
  int v7; // eax
  int result; // eax

  sub_3702F(a1, a2, a4, a3, 40);
  v7 = *(unsigned __int8 *)(dword_53AD5 + 16);
  if ( v7 == 1 )
  {
    sub_15F84(1, v5, v6, a4, a5, dword_53A79, 1, 655360, 320, 205, 76, 74, 19, 1);
    sub_35B78(9, 44, 3);
    sub_35B78(0, 9, 4);
    sub_35B78(17, 9, 5);
    dword_51A83 = 1;
  }
  else if ( v7 == 2 )
  {
    sub_15F84(2, v5, v6, a4, a5, dword_53A79, 2, 655360, 320, 205, 76, 74, 19, 1);
    sub_35F10(16);
  }
  result = dword_53AD5;
  ++*(_BYTE *)(dword_53AD5 + 16);
  return result;
}


// ===== 35CF1 =====
int __fastcall sub_35CF1(__int32 a1, int a2, int a3, int a4)
{
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 4);
  result = dword_53AD5;
  if ( *(_BYTE *)(dword_53AD5 + 16) == 0 )
  {
    *(_BYTE *)(dword_53A55 + 3) = dword_53BEF;
    result = dword_53AD5;
    *(_BYTE *)(dword_53AD5 + 16) = 1;
  }
  return result;
}


// ===== 35D1E =====
void __usercall sub_35D1E(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  __int32 v8; // eax
  int v9; // edx
  int v10; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  sub_15F84(v5, v6, v7, a4, a5, dword_53A79, 3, 655360, 320, 205, 76, 74, 19, 1);
  v8 = sub_35B78(17, 18, 1);
  sub_15F84(v8, v9, v10, a4, a5, dword_53A79, 6, 655360, 320, 205, 76, 74, 19, 1);
}


// ===== 35D9E =====
int __usercall sub_35D9E@<eax>(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  __int32 v8; // eax
  int v9; // edx
  int v10; // ecx
  int result; // eax

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  sub_15F84(v5, v6, v7, a4, a5, dword_53A79, 4, 655360, 320, 205, 76, 74, 19, 1);
  v8 = sub_35B78(14, 7, 2);
  sub_15F84(v8, v9, v10, a4, a5, dword_53A79, 6, 655360, 320, 205, 76, 74, 19, 1);
  result = dword_53AD5;
  *(_BYTE *)(dword_53AD5 + 18) = 1;
  return result;
}


// ===== 35E0E =====
int __fastcall sub_35E0E(__int32 a1, int a2, int a3, int a4, int a5)
{
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 4);
  result = dword_53A45;
  if ( *(_BYTE *)(80 * a5 + dword_53A45 + 6) != 0 )
  {
    result = dword_53AD5;
    if ( *(_BYTE *)(dword_53AD5 + 17) == 0 && *(_BYTE *)(dword_53AD5 + 18) != 0 )
    {
      *(_BYTE *)(dword_53A55 + 9) = dword_53BEF;
      result = dword_53AD5;
      *(_BYTE *)(dword_53AD5 + 17) = 1;
    }
  }
  return result;
}


// ===== 35E5B =====
void __usercall sub_35E5B(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // eax
  int v9; // edx
  int v10; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  v8 = sub_344F2(v5, v6, a4, v7, 41, 45, 0);
  sub_15F84(v8, v9, v10, a4, a5, dword_53A79, 5, 655360, 320, 205, 76, 74, 19, 1);
  sub_35B78(8, 7, 3);
  sub_35B78(4, 7, 4);
  JUMPOUT(0x35D55);
}


// ===== 35EC1 =====
int __usercall sub_35EC1@<eax>(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // edx
  int v6; // ecx
  int result; // eax

  sub_3702F(a1, a2, a4, a3, 40);
  if ( *(_BYTE *)(dword_53AD5 + 19) != 0 )
  {
    sub_15F84(dword_53AD5, v5, v6, a4, a5, dword_53A79, 2, 655360, 320, 205, 76, 74, 19, 1);
    sub_35F10(20);
  }
  result = dword_53AD5;
  ++*(_BYTE *)(dword_53AD5 + 19);
  return result;
}


// ===== 35F48 =====
int __fastcall sub_35F48(__int32 a1, int a2, int a3, int a4)
{
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 16);
  sub_35B78(4, 35, 2);
  result = sub_35B78(14, 35, 3);
  dword_51A83 = 1;
  return result;
}


// ===== 35F88 =====
void __fastcall sub_35F88(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 16);
  sub_35B78(10, 29, *(_BYTE *)(dword_53AD5 + 16));
  if ( *(_BYTE *)(dword_53AD5 + 16) != 7 )
    *(_BYTE *)(dword_53A55 + 3) = dword_53BEF + 1;
  JUMPOUT(0x35CE8);
}


// ===== 35FCF =====
_BYTE *__usercall sub_35FCF@<eax>(
        __int32 a1@<eax>,
        int a2@<edx>,
        int a3@<ecx>,
        int a4@<ebx>,
        unsigned __int8 *a5@<edi>,
        int a6)
{
  int v6; // ecx
  _BYTE *result; // eax
  int v8; // edx
  __int32 v9; // eax
  __int32 v10; // eax
  int v11; // edx
  int v12; // ecx
  __int16 *v13; // eax
  int v14; // edx
  int v15; // ecx
  int v16; // eax

  sub_3702F(a1, a2, a4, a3, 40);
  result = (_BYTE *)(80 * a6 + dword_53A45);
  if ( result[6] != 0 && *(_BYTE *)(dword_53AD5 + 17) == 0 )
  {
    v8 = (unsigned __int8)result[8];
    if ( v8 == 9 )
    {
      sub_15F84((__int32)result, 9, v6, a4, a5, dword_53A79, 1, 655360, 320, 205, 76, 74, 19, 1);
      *(_BYTE *)(dword_53AD5 + 17) = 1;
      *(_BYTE *)(dword_53A55 + 6) = dword_53BEF + 1;
      *(_BYTE *)(dword_53AD5 + 16) = 4;
      result = (_BYTE *)dword_53A55;
      *(_BYTE *)(dword_53A55 + 3) = dword_53BEF;
    }
    else
    {
      v9 = sub_1956B((unsigned __int8)result[7]);
      sub_15F84(v9, v8, v6, a4, a5, dword_53A79, 0, 693535, 320, 205, 76, 74, 19, 1);
      v13 = sub_16559(v10, v11, a4, v12, 0);
      sub_16C57((__int32)v13, v14, a4, v15, 0);
      return (_BYTE *)sub_196CB(v16);
    }
  }
  return result;
}


// ===== 360B6 =====
void __usercall sub_360B6(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  __int32 v8; // eax
  int v9; // edx
  int v10; // ecx
  int v11; // eax
  int v12; // ebx
  int v13; // edx
  int v14; // ecx
  __int32 v15; // eax
  int v16; // edx
  int v17; // ecx
  int v18; // edx
  int v19; // ecx
  __int32 v20; // eax
  int v21; // eax
  int v22; // edx
  int v23; // ecx
  unsigned __int8 i; // [esp+0h] [ebp-8h]

  v5 = sub_3702F(a1, a2, a4, a3, 48);
  if ( *(_BYTE *)(dword_53AD5 + 17) == 4 )
  {
    sub_15F84(v5, v6, v7, 4, a5, dword_53A79, 2, 655360, 320, 205, 76, 74, 19, 1);
    v11 = sub_10B4E(v8, v9, 4, v10, 1);
    *(_BYTE *)(dword_53AD5 + 21) = dword_53BEB - 3;
    v12 = dword_53A55;
    LOBYTE(v11) = dword_53BEF;
    *(_BYTE *)(dword_53A55 + 9) = dword_53BEF;
    sub_361B0(v11, v13, v12, v14);
    v15 = j___delay(400);
    sub_361B0(v15, v16, v12, v17);
    v20 = j___delay(400);
    for ( i = 3; i <= 6u; ++i )
    {
      v21 = sub_361B0(v20, v18, i, v19);
      sub_15F84(v21, v22, v23, i, a5, dword_53A79, i, 655360, 320, 205, 76, 74, 19, 1);
    }
  }
  else
  {
    sub_13512(1);
    ++*(_BYTE *)(dword_53AD5 + 17);
    *(_BYTE *)(dword_53A55 + 6) = dword_53BEF + 1;
  }
}


// ===== 3623C =====
int __fastcall sub_3623C(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // ebx

  sub_3702F(a1, a2, a3, a4, 16);
  *(_BYTE *)(dword_53A55 + 9) = dword_53BEF + 1;
  LOWORD(v4) = sub_4EBE3();
  v5 = v4;
  sub_13512(*(unsigned __int8 *)(dword_53AD5 + 21) + v4 % 3);
  return sub_13512(*(unsigned __int8 *)(dword_53AD5 + 21) + (v5 + 1) % 3);
}


// ===== 362E8 =====
void __usercall sub_362E8(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // edx
  int v9; // ecx
  __int32 v10; // eax
  int v11; // edx
  int v12; // ecx
  int v13; // ecx
  int v14; // edx
  __int32 v15; // eax
  int v16; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  sub_135DD(v5, v6, a4, v7, 16, 1);
  sub_15F84(
    *(unsigned __int8 *)(dword_53AD5 + 16) + 2,
    v8,
    v9,
    a4,
    a5,
    dword_53A79,
    *(unsigned __int8 *)(dword_53AD5 + 16) + 2,
    655360,
    320,
    205,
    76,
    74,
    19,
    1);
  dword_51A83 = 0;
  sub_135DD(v10, v11, a4, v12, 16, 14);
  v14 = 24 - *(unsigned __int8 *)(dword_53AD5 + 16);
  sub_33F78(v14, 22, 18);
  v15 = *(unsigned __int8 *)(dword_53AD5 + 16);
  if ( v15 == 4 )
  {
    sub_344F2(4, v14, a4, v13, 20, 20, 11);
  }
  else
  {
    sub_10B4E(v15, v14, a4, v13, *(unsigned __int8 *)(dword_53AD5 + 16));
    sub_344F2(
      *(unsigned __int8 *)(dword_53AD5 + 16),
      24 - *(unsigned __int8 *)(dword_53AD5 + 16),
      a4,
      v16,
      24 - *(unsigned __int8 *)(dword_53AD5 + 16),
      24 - *(unsigned __int8 *)(dword_53AD5 + 16),
      0);
    sub_33F78(2 * *(unsigned __int8 *)(dword_53AD5 + 16) + 25, 21, 18);
    sub_33F78(2 * *(unsigned __int8 *)(dword_53AD5 + 16) + 26, 23, 18);
  }
  JUMPOUT(0x35F6B);
}


// ===== 35B78 =====
void __fastcall sub_35B78(__int32 a1, int a2, int a3, int a4, int a5, int a6, unsigned __int8 a7)
{
  int v7; // eax
  int v8; // edx
  int v9; // ecx
  int v10; // edx
  int v11; // ecx
  __int32 v12; // eax
  int v13; // edx
  int v14; // ecx

  v7 = sub_3702F(a1, a2, a3, a4, 16);
  sub_135DD(v7, v8, a3, v9, a5, a6);
  sub_10B4E(a7, v10, a3, v11, a7);
  j___delay(300);
  sub_11DF2(0, 255, 255);
  j___delay(200);
  v12 = sub_11DF2(0, 255, 0);
  sub_11CAC(v12, v13, a3, v14, 0);
  JUMPOUT(0x35722);
}


// ===== 35F10 =====
__int64 __fastcall sub_35F10(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // eax

  v5 = sub_3702F(a1, a2, a3, a4, 8);
  while ( a5 < dword_53BEB )
  {
    v5 = 80 * a5;
    *(_WORD *)(dword_53A45 + 80 * a5++ + 64) = 0;
  }
  return sub_1DB65(v5);
}


// ===== 361B0 =====
int __fastcall sub_361B0(__int32 a1, int a2, int a3, int a4)
{
  int i; // ebx
  int result; // eax
  int j; // ebx

  sub_3702F(a1, a2, a3, a4, 20);
  for ( i = 0; i < 64; ++i )
  {
    sub_11DF2(0, 255, i);
    j___delay(8);
  }
  result = j___delay(400);
  for ( j = 62; j >= 0; --j )
  {
    sub_11DF2(0, 255, j);
    result = j___delay(8);
  }
  return result;
}


// ===== 1AA1D =====
void __fastcall sub_1AA1D(__int32 a1, int a2, int a3, int a4, unsigned __int8 *a5, int a6, int a7)
{
  int v7; // edx
  int v8; // ecx
  int v9; // esi
  int i; // ebp
  __int16 *v11; // eax
  int v12; // edx
  int v13; // ecx
  int v14; // eax
  __int32 v15; // eax
  int v16; // edx
  int v17; // ecx
  __int32 v18; // eax
  int v19; // edx
  int v20; // ecx
  __int16 *v21; // eax
  __int64 v22; // rax
  __int32 v23; // eax
  int v24; // edx
  int v25; // ecx
  __int32 v26; // eax
  int v27; // eax
  __int32 v28; // eax
  __int32 v29; // eax
  int v30; // edx
  int v31; // ecx
  __int16 *v32; // eax
  int v33; // edx
  int v34; // ecx
  int v35; // eax
  unsigned __int8 *v36; // eax
  int v37; // ebx
  int v38; // eax
  __int32 v39; // eax
  int v40; // edx
  int v41; // ecx
  __int32 v42; // eax
  __int16 *v43; // eax
  int v44; // edx
  int v45; // ecx
  int v46; // [esp+0h] [ebp-14h]

  sub_3702F(a1, a2, a3, a4, 60);
  if ( a6 == 0 )
LABEL_24:
    JUMPOUT(0x22BBE);
  v9 = 80 * (_DWORD)a5 + dword_53A45;
  for ( i = 0; ; ++i )
  {
    if ( i >= a6 )
      goto LABEL_24;
    sub_4E381();
    v36 = (unsigned __int8 *)(a7 + 3 * i);
    v37 = *(unsigned __int16 *)(v36 + 1);
    v38 = *v36;
    if ( v38 != 0 )
    {
      switch ( v38 )
      {
        case 1:
          if ( *(_BYTE *)(v9 + 6) != 2 )
            goto LABEL_24;
          v28 = sub_1956B(*(unsigned __int8 *)(v9 + 7));
          dword_53AE1 = v37;
          sub_15F84(v28, v7, v8, v37, a5, dword_53A7D, 435, 696099, 320, 205, 76, 74, 19, 1);
          v32 = sub_16559(v29, v30, v37, v31, 0);
          sub_16C57((__int32)v32, v33, v37, v34, 0);
          sub_196CB(v35);
          dword_53BF3 += dword_53AE1;
          break;
        case 2:
          j___delay(200);
          ((void (__cdecl *)(unsigned __int8 *))funcs_1199C[v37])(a5);
          break;
        case 3:
          sub_15F84(3, v7, v8, v37, a5, dword_53A79, v37, 655360, 320, 205, 76, 74, 19, 1);
          break;
        default:
          break;
      }
    }
    else
    {
      if ( *(_BYTE *)(v9 + 6) != 2 )
        goto LABEL_24;
      dword_53AD9 = v37 + 181;
      v39 = sub_1956B(*(unsigned __int8 *)(v9 + 7));
      sub_15F84(v39, v7, v8, v37, a5, dword_53A7D, 432, 696099, 320, 205, 76, 74, 19, 1);
      v42 = sub_1BB8C(a5, v37);
      if ( v42 == -1 )
      {
        v11 = sub_16559(-1, v40, v37, v41, 0);
        sub_16C57((__int32)v11, v12, v37, v13, 0);
        sub_196CB(v14);
        j___delay(100);
        v15 = sub_1956B(*(unsigned __int8 *)(v9 + 7));
        sub_15F84(v15, v16, v17, v37, a5, dword_53A7D, 433, 696099, 320, 205, 76, 74, 19, 1);
        v21 = sub_16559(v18, v19, v37, v20, 0);
        v22 = sub_19953(v21);
        v46 = v22;
        v23 = sub_197E5(v22, HIDWORD(v22), v37);
        if ( v46 != 1 || dword_53C57 != 0 )
        {
          sub_15F84(v23, v24, v25, v37, a5, dword_53A7D, 434, 702179, 320, 205, 76, 74, 19, 1);
        }
        else
        {
          sub_196CB(v23);
          if ( sub_1B932(a5, 0) != 0 )
          {
            sub_1B722(a5, dword_53C57);
            sub_1B8E7(a5, dword_53C57);
            sub_1BB8C(a5, v37);
            continue;
          }
          j___delay(100);
          v26 = sub_1956B(*(unsigned __int8 *)(v9 + 7));
          sub_15F84(v26, v7, v8, v37, a5, dword_53A7D, 434, 696099, 320, 205, 76, 74, 19, 1);
        }
        v27 = j___delay(200);
      }
      else
      {
        v43 = sub_16559(v42, v40, v37, v41, 0);
        sub_16C57((__int32)v43, v44, v37, v45, 0);
      }
      sub_196CB(v27);
    }
  }
}


// ===== 2AEDB =====
int __fastcall sub_2AEDB(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6; // esi
  int i; // ebx

  sub_3702F(a1, a2, a3, a4, 28);
  v6 = sub_1B8A6(a5);
  if ( v6 != 0 )
  {
    for ( i = 0; i < v6; ++i )
    {
      if ( sub_1B722(a5, i) == a6 )
        return i;
    }
  }
  return -1;
}


// ===== 33F78 =====
None