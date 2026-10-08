
// ===== 34B07 =====
void __fastcall sub_34B07(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 40);
  JUMPOUT(0x34885);
}


// ===== 34C52 =====
void __fastcall sub_34C52(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 40);
  JUMPOUT(0x34F65);
}


// ===== 34940 =====
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


// ===== 34A1E =====
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


// ===== 34FC2 =====
void sub_34FC2()
{
  JUMPOUT(0x34C57);
}


// ===== 350BE =====
void sub_350BE()
{
  JUMPOUT(0x34F3D);
}


// ===== 205BE =====
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


// ===== 12CEA =====
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


// ===== 34531 =====
int __usercall sub_34531@<eax>(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  __int32 v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // eax
  int v9; // edx
  int v10; // ecx
  __int32 v11; // eax
  int v12; // edx
  int v13; // ecx
  __int32 v14; // eax
  int v15; // edx
  int v16; // ecx
  __int32 v17; // eax
  int v18; // edx
  int v19; // ecx
  __int32 v20; // eax
  int v21; // edx
  int v22; // ecx
  int v23; // eax
  int v24; // edx
  int v25; // ecx
  __int32 v26; // eax
  int v27; // edx
  int v28; // ecx
  __int32 v29; // eax
  int v30; // edx
  int v31; // ecx

  sub_3702F(a1, a2, a4, a3, 44);
  v5 = sub_112A5(1);
  v8 = sub_10B4E(v5, v6, a4, v7, 3);
  sub_135DD(v8, v9, a4, v10, 5, 8);
  sub_11CAC(v11, v12, a4, v13, 1);
  v14 = j___delay(100);
  sub_1366A(v14, v15, a4, v16, 7);
  LOWORD(v17) = sub_4E381();
  sub_15F84(v17, v18, v19, a4, a5, dword_53A79, 11, 655360, 320, 205, 76, 74, 19, 1);
  dword_51A83 = 0;
  v23 = sub_10B4E(v20, v21, a4, v22, 7);
  sub_11CAC(v23, v24, a4, v25, 1);
  v26 = j___delay(100);
  sub_1366A(v26, v27, a4, v28, 8);
  LOWORD(v29) = sub_4E381();
  sub_15F84(v29, v30, v31, a4, a5, dword_53A79, 3, 655360, 320, 205, 76, 74, 19, 1);
  return sub_134E4();
}


// ===== 3460B =====
void __usercall sub_3460B(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  __int32 v8; // eax
  int v9; // edx
  int v10; // ecx
  __int32 v11; // eax
  int v12; // edx
  int v13; // ecx
  int v14; // eax
  int v15; // edx
  int v16; // ecx
  int v17; // eax
  __int32 v18; // eax
  int v19; // edx
  int v20; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 44);
  sub_135DD(v5, v6, a4, v7, 11, 16);
  sub_32999(v8, v9, a4, v10, 4);
  LOWORD(v11) = sub_4E381();
  v14 = sub_11CAC(v11, v12, a4, v13, 1);
  v17 = sub_1366A(v14, v15, a4, v16, 3);
  v18 = sub_134E4(v17);
  sub_15F84(v18, v19, v20, a4, a5, dword_53A79, 4, 655360, 320, 205, 76, 74, 19, 1);
}


// ===== 34673 =====
void __fastcall sub_34673(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  __int32 v7; // eax
  int v8; // edx
  int v9; // ecx
  __int32 v10; // eax
  int v11; // edx
  int v12; // ecx
  int v13; // eax
  int v14; // edx
  int v15; // ecx
  int v16; // eax

  v4 = sub_3702F(a1, a2, a3, a4, 44);
  sub_135DD(v4, v5, a3, v6, 0, 16);
  sub_32999(v7, v8, a3, v9, 5);
  LOWORD(v10) = sub_4E381();
  v13 = sub_11CAC(v10, v11, a3, v12, 1);
  v16 = sub_1366A(v13, v14, a3, v15, 4);
  sub_134E4(v16);
  JUMPOUT(0x34663);
}


// ===== 346CD =====
void __fastcall sub_346CD(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  __int32 v7; // eax
  int v8; // edx
  int v9; // ecx
  int v10; // eax
  int v11; // edx
  int v12; // ecx
  int v13; // eax
  int v14; // edx
  int v15; // ecx
  int v16; // eax

  v4 = sub_3702F(a1, a2, a3, a4, 44);
  sub_135DD(v4, v5, a3, v6, 11, 11);
  byte_53AFA = 1;
  v10 = sub_10B4E(v7, v8, a3, v9, 6);
  byte_53AFA = 0;
  v13 = sub_11CAC(v10, v11, a3, v12, 1);
  v16 = sub_1366A(v13, v14, a3, v15, 6);
  sub_134E4(v16);
  sub_4E381();
  JUMPOUT(0x34663);
}


// ===== 34778 =====
void __usercall sub_34778(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
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
  __int32 v14; // eax
  int v15; // edx
  int v16; // ecx
  int i; // edx
  int v18; // ebx
  int v19; // eax

  v5 = sub_3702F(a1, a2, a4, a3, 44);
  sub_135DD(v5, v6, a4, v7, 9, 1);
  v8 = j___delay(100);
  byte_53AFA = 1;
  v11 = sub_10B4E(v8, v9, a4, v10, 3);
  byte_53AFA = 0;
  sub_1366A(v11, v12, a4, v13, 13);
  v14 = j___delay(200);
  sub_15F84(v14, v15, v16, a4, a5, dword_53A79, 4, 655360, 320, 205, 76, 74, 19, 1);
  for ( i = 5; i < 11; ++i )
  {
    v18 = 80 * i;
    v19 = dword_53A45;
    *(_BYTE *)(v18 + dword_53A45 + 53) = 26;
    *(_BYTE *)(v18 + v19 + 54) = 15;
  }
}


// ===== 348BB =====
void __fastcall sub_348BB(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 40);
  sub_10B4E(v4, v5, a3, v6, 2);
  JUMPOUT(0x34885);
}


// ===== 34984 =====
void __fastcall sub_34984(__int32 a1, int a2, int a3, int a4)
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
  int v13; // eax
  __int32 v14; // eax
  int v15; // edx
  int v16; // ecx
  int v17; // eax
  int v18; // edx
  int v19; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 40);
  dword_51A83 = 0;
  byte_53AFA = 1;
  v7 = sub_10B4E(v4, v5, a3, v6, 2);
  byte_53AFA = 0;
  sub_135DD(v7, v8, a3, v9, 14, 0);
  v13 = sub_1366A(v10, v11, a3, v12, 23);
  v14 = sub_134E4(v13);
  v17 = sub_344F2(v14, v15, a3, v16, 7, 12, 0);
  sub_344F2(v17, v18, a3, v19, 33, 35, 0);
  JUMPOUT(0x3486C);
}


// ===== 349EC =====
void __fastcall sub_349EC(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 40);
  sub_10B4E(v4, v5, a3, v6, 3);
  JUMPOUT(0x34885);
}


// ===== 34B6F =====
void __fastcall sub_34B6F(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  int v7; // edx
  int v8; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 40);
  if ( sub_34894(v4, v5, a3, v6, 8) == 0 )
  {
    sub_10B4E(0, v7, a3, v8, 1);
    JUMPOUT(0x34C5C);
  }
  JUMPOUT(0x34F73);
}


// ===== 34B9A =====
void __usercall sub_34B9A(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
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
  int v14; // eax
  int v15; // edx
  int v16; // ecx
  __int32 v17; // eax
  int v18; // edx
  int v19; // ecx
  int v20; // eax
  int v21; // edx
  int v22; // ecx
  __int32 v23; // eax
  int v24; // edx
  int v25; // ecx
  __int32 v26; // eax
  int v27; // edx
  int v28; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  v8 = sub_344F2(v5, v6, a4, v7, 8, 28, 0);
  sub_15F84(v8, v9, v10, a4, a5, dword_53A79, 4, 655360, 320, 205, 76, 74, 19, 1);
  if ( dword_53BEF < 15 )
  {
    v14 = sub_10B4E(v11, v12, a4, v13, 2);
    sub_135DD(v14, v15, a4, v16, 5, 17);
    v20 = sub_1366A(v17, v18, a4, v19, 25);
    sub_15F84(v20, v21, v22, a4, a5, dword_53A79, 5, 655360, 320, 205, 76, 74, 19, 1);
    sub_135DD(v23, v24, a4, v25, 5, 17);
    sub_1366A(v26, v27, a4, v28, 26);
    sub_32975(33);
    JUMPOUT(0x35F6E);
  }
  JUMPOUT(0x35F78);
}


// ===== 34C7A =====
int __usercall sub_34C7A@<eax>(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // edx
  int v6; // ecx
  int result; // eax
  int v8; // eax
  int v9; // edx
  int v10; // ecx
  __int32 v11; // eax
  int v12; // edx
  int v13; // ecx
  int v14; // eax
  int v15; // edx
  int v16; // ecx

  sub_3702F(a1, a2, a4, a3, 40);
  result = *(unsigned __int8 *)(dword_53AD5 + 16);
  if ( result == 1 )
  {
    byte_53AFA = 1;
    v8 = sub_10B4E(1, v5, a4, v6, 2);
    byte_53AFA = 0;
    sub_135DD(v8, v9, a4, v10, 16, 10);
    v14 = sub_1366A(v11, v12, a4, v13, 30);
    sub_15F84(v14, v15, v16, a4, a5, dword_53A79, 2, 655360, 320, 205, 76, 74, 19, 1);
    result = dword_53AD5;
    *(_BYTE *)(dword_53AD5 + 17) = 1;
  }
  return result;
}


// ===== 34D2F =====
void __fastcall sub_34D2F(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  __int32 v7; // eax
  int v8; // edx
  int v9; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 12);
  sub_135DD(v4, v5, a3, v6, 8, 2);
  v7 = j___delay(100);
  sub_10B4E(v7, v8, a3, v9, dword_53BEF);
  JUMPOUT(0x35727);
}


// ===== 34F38 =====
void __usercall sub_34F38(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // eax
  int v9; // edx
  int v10; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  v8 = sub_10B4E(v5, v6, a4, v7, 1);
  sub_15F84(v8, v9, v10, a4, a5, dword_53A79, 1, 655360, 320, 205, 76, 74, 19, 1);
}


// ===== 34FCC =====
int __fastcall sub_34FCC(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  __int32 v7; // eax
  int v8; // edx
  int v9; // ecx
  int v10; // eax
  int v11; // edx
  int v12; // ecx
  int v13; // eax

  v4 = sub_3702F(a1, a2, a3, a4, 12);
  sub_135DD(v4, v5, a3, v6, 12, 5);
  byte_53AFA = 1;
  v10 = sub_10B4E(v7, v8, a3, v9, 2);
  byte_53AFA = 0;
  v13 = sub_1366A(v10, v11, a3, v12, 42);
  return sub_134E4(v13);
}


// ===== 35022 =====
void __usercall sub_35022(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  __int32 v8; // eax
  int v9; // edx
  int v10; // ecx
  __int32 v11; // eax
  int v12; // edx
  int v13; // ecx
  int v14; // eax
  int v15; // edx
  int v16; // ecx
  int v17; // eax
  __int32 v18; // eax
  int v19; // edx
  int v20; // ecx
  __int32 v21; // eax
  int v22; // edx
  int v23; // ecx
  int v24; // eax
  int v25; // edx
  int v26; // ecx
  int v27; // eax

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  sub_15F84(v5, v6, v7, a4, a5, dword_53A79, 1, 655360, 320, 205, 76, 74, 19, 1);
  sub_135DD(v8, v9, a4, v10, 15, 34);
  byte_53AFA = 1;
  v14 = sub_10B4E(v11, v12, a4, v13, 3);
  byte_53AFA = 0;
  v17 = sub_1366A(v14, v15, a4, v16, 43);
  v18 = sub_134E4(v17);
  sub_135DD(v18, v19, a4, v20, 0, 26);
  byte_53AFA = 1;
  v24 = sub_10B4E(v21, v22, a4, v23, 4);
  byte_53AFA = 0;
  v27 = sub_1366A(v24, v25, a4, v26, 44);
  sub_134E4(v27);
  JUMPOUT(0x35F6E);
}


// ===== 350C8 =====
void __fastcall sub_350C8(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx
  __int32 v7; // eax
  int v8; // edx
  int v9; // ecx
  int v10; // eax
  int v11; // edx
  int v12; // ecx
  int v13; // eax

  v4 = sub_3702F(a1, a2, a3, a4, 40);
  sub_135DD(v4, v5, a3, v6, 27, 5);
  byte_53AFA = 1;
  v10 = sub_10B4E(v7, v8, a3, v9, 2);
  byte_53AFA = 0;
  v13 = sub_1366A(v10, v11, a3, v12, 46);
  sub_134E4(v13);
  JUMPOUT(0x34F65);
}


// ===== 34818 =====
void __usercall sub_34818(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // edx
  int v9; // ecx
  int v10; // eax
  int v11; // edx
  int v12; // ecx
  __int32 v13; // eax
  int v14; // edx
  int v15; // ecx
  __int32 v16; // eax
  int v17; // edx
  int v18; // ecx

  v5 = sub_3702F(a1, a2, a4, a3, 40);
  if ( sub_34894(v5, v6, a4, v7, 6) == 0 )
  {
    v10 = sub_10B4E(0, v8, a4, v9, 2);
    sub_135DD(v10, v11, a4, v12, 3, 0);
    v13 = j___delay(800);
    sub_135DD(v13, v14, a4, v15, 3, 17);
    v16 = j___delay(200);
    sub_15F84(v16, v17, v18, a4, a5, dword_53A79, 4, 655360, 320, 205, 76, 74, 19, 1);
  }
}
