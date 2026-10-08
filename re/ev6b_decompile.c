// ===== 0x34531 sub_34531 size=218 =====
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


// ===== 0x3460b sub_3460B size=104 =====
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


// ===== 0x34673 sub_34673 size=90 =====
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


// ===== 0x346cd sub_346CD size=107 =====
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


// ===== 0x34778 sub_34778 size=160 =====
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


// ===== 0x350be sub_350BE size=10 =====
void sub_350BE()
{
  JUMPOUT(0x34F3D);
}


// ===== 0x350c8 sub_350C8 size=91 =====
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


// ===== 0x34818 sub_34818 size=124 =====
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


// ===== 0x348bb sub_348BB size=47 =====
void __fastcall sub_348BB(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 40);
  sub_10B4E(v4, v5, a3, v6, 2);
  JUMPOUT(0x34885);
}


// ===== 0x34940 sub_34940 size=68 =====
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


// ===== 0x34984 sub_34984 size=104 =====
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


// ===== 0x349ec sub_349EC size=50 =====
void __fastcall sub_349EC(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 40);
  sub_10B4E(v4, v5, a3, v6, 3);
  JUMPOUT(0x34885);
}


// ===== 0x34a1e sub_34A1E size=78 =====
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


// ===== 0x34b07 sub_34B07 size=40 =====
void __fastcall sub_34B07(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 40);
  JUMPOUT(0x34885);
}


// ===== 0x34b6f sub_34B6F size=43 =====
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


// ===== 0x34b9a sub_34B9A size=184 =====
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


// ===== 0x34c52 sub_34C52 size=40 =====
void __fastcall sub_34C52(__int32 a1, int a2, int a3, int a4)
{
  sub_3702F(a1, a2, a3, a4, 40);
  JUMPOUT(0x34F65);
}


// ===== 0x34c7a sub_34C7A size=119 =====
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


// ===== 0x34d2f sub_34D2F size=53 =====
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


// ===== 0x34dd0 sub_34DD0 size=227 =====
int __usercall sub_34DD0@<eax>(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // ecx
  int i; // edx
  __int32 v7; // eax
  __int32 v8; // eax
  int v9; // edx
  int v10; // ecx
  int v11; // eax
  int v12; // edx
  int v13; // ecx
  int result; // eax

  sub_3702F(a1, a2, a4, a3, 44);
  for ( i = 12; i < 34; ++i )
  {
    a4 = 80 * i;
    *(_BYTE *)(80 * i + dword_53A45 + 52) = 0;
  }
  *(_BYTE *)(dword_53A55 + 3) = dword_53BEF + 1;
  LOBYTE(i) = dword_53BEF + 2;
  *(_BYTE *)(dword_53A55 + 6) = dword_53BEF + 2;
  v7 = dword_53A45 + 880;
  *(_BYTE *)(dword_53A45 + 885) = 0;
  *(_BYTE *)(v7 + 6) = 1;
  *(_BYTE *)(v7 + 7) = 6;
  *(_BYTE *)(v7 + 8) = 6;
  *(_BYTE *)(v7 + 49) = -1;
  *(_BYTE *)(v7 + 52) = 0x80;
  *(_WORD *)(v7 + 64) = 1;
  sub_15F84(v7, i, v5, a4, a5, dword_53A79, 2, 655360, 320, 205, 76, 74, 19, 1);
  v11 = sub_10B4E(v8, v9, a4, v10, 1);
  sub_15F84(v11, v12, v13, a4, a5, dword_53A79, 3, 655360, 320, 205, 76, 74, 19, 1);
  dword_53EC8 = 0;
  result = dword_53AD5;
  *(_BYTE *)(dword_53AD5 + 16) = 2;
  return result;
}


// ===== 0x34eb3 sub_34EB3 size=133 =====
void __fastcall sub_34EB3(__int32 a1, int a2, int a3, int a4)
{
  int v4; // edx
  int v5; // ecx
  __int32 v6; // eax
  int v7; // edx
  int v8; // ecx
  __int32 v9; // eax
  int v10; // edx
  int v11; // ecx
  __int32 v12; // eax
  int v13; // edx
  int v14; // ecx
  __int32 v15; // eax
  int v16; // edx
  int v17; // ecx

  sub_3702F(a1, a2, a3, a4, 12);
  sub_10B4E(*(unsigned __int8 *)(dword_53AD5 + 16), v4, a3, v5, *(unsigned __int8 *)(dword_53AD5 + 16));
  v6 = dword_53AD5;
  ++*(_BYTE *)(dword_53AD5 + 16);
  sub_135DD(v6, v7, a3, v8, 0, 0);
  v9 = j___delay(200);
  sub_135DD(v9, v10, a3, v11, 12, 0);
  v12 = j___delay(200);
  sub_135DD(v12, v13, a3, v14, 12, 11);
  v15 = j___delay(200);
  sub_135DD(v15, v16, a3, v17, 0, 11);
  JUMPOUT(0x35727);
}


// ===== 0x34f38 sub_34F38 size=60 =====
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


// ===== 0x34fc2 sub_34FC2 size=10 =====
void sub_34FC2()
{
  JUMPOUT(0x34C57);
}


// ===== 0x34fcc sub_34FCC size=61 =====
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


// ===== 0x35022 sub_35022 size=156 =====
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


// ===== 0x32999 sub_32999 size=895 =====
int __fastcall sub_32999(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // eax
  int v6; // edx
  int v7; // ecx
  int v8; // edx
  int v9; // ecx
  __int64 v10; // rax
  int v11; // edi
  int v12; // ecx
  int v13; // eax
  int v14; // edx
  int v15; // ecx
  int j; // esi
  unsigned __int8 *v17; // eax
  int k; // esi
  int v19; // eax
  __int32 v20; // eax
  __int16 *v21; // ebp
  int m; // esi
  __int32 v23; // eax
  _BYTE *v25; // [esp+0h] [ebp-20h]
  _BYTE *v26; // [esp+4h] [ebp-1Ch]
  int v27; // [esp+8h] [ebp-18h]
  int i; // [esp+Ch] [ebp-14h]

  v5 = sub_3702F(a1, a2, a3, a4, 60);
  v25 = sub_111BA(v5, v6, a3, v7, (int)aFdotherDat, 0, 95);
  v26 = sub_111BA((__int32)v25, v8, a3, v9, (int)aFdotherDat, 0, 9);
  v10 = malloc(153216);
  v27 = v10;
  LODWORD(v10) = memmove(v10, dword_53A49, 153216);
  v11 = dword_53BEB;
  v13 = sub_10B4E(v10, SHIDWORD(v10), a3, v12, a5);
  for ( i = 0; i < 12; ++i )
  {
    if ( i == 1 )
      sub_25A96(v13, v14, a3, v15, (int)v25, 0, 1);
    memmove(dword_53A49, v27, 153216);
    v21 = (__int16 *)&v26[*(_DWORD *)&v26[4 * i + 6]];
    for ( j = v11; j < dword_53BEB; ++j )
    {
      v17 = (unsigned __int8 *)(dword_53A45 + 80 * j);
      v14 = *v17;
      a3 = v17[1];
      if ( v14 >= dword_53AA9 - 1
        && v14 <= dword_51A87 + dword_53AA9
        && a3 >= dword_53AAD
        && a3 <= dword_51A8B + dword_53AAD + 1 )
      {
        v15 = dword_53A49 + 32904 + 24 * (v14 - dword_53AA9 - 1);
        v14 = 1216 * (a3 - dword_53AAD);
        sub_4EBAB((_BYTE *)(v15 + 10944 * (a3 - dword_53AAD) - 2736), v21, 456);
      }
    }
    sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
    switch ( i )
    {
      case 6:
        memmove(dword_53A49, v27, 153216);
        for ( k = 0; k < v11; ++k )
        {
          if ( (*(_BYTE *)(80 * k + dword_53A45 + 5) & 1) == 0 )
            sub_127E0(k);
        }
        dword_53A49 -= 3648;
        while ( k < dword_53BEB )
        {
          if ( (*(_BYTE *)(dword_53A45 + 80 * k + 5) & 1) == 0 )
            sub_127E0(k);
          ++k;
        }
        v19 = dword_53A49 + 3648;
        goto LABEL_21;
      case 7:
        sub_11EEE(dword_53A49 + 32904, 456, 13, 8, dword_53AA9, dword_53AAD);
        for ( m = 0; m < v11; ++m )
        {
          if ( (*(_BYTE *)(dword_53A45 + 80 * m + 5) & 1) == 0 )
            sub_127E0(m);
        }
        dword_53A49 -= 2280;
        while ( m < dword_53BEB )
        {
          if ( (*(_BYTE *)(80 * m + dword_53A45 + 5) & 1) == 0 )
            sub_127E0(m);
          ++m;
        }
        v19 = dword_53A49 + 2280;
LABEL_21:
        dword_53A49 = v19;
        sub_129EC(v19);
LABEL_22:
        memmove(v27, dword_53A49, 153216);
        break;
      case 8:
        v23 = sub_11EEE(dword_53A49 + 32904, 456, 13, 8, dword_53AA9, dword_53AAD);
        sub_127A9(v23, v14, a3, v15);
        goto LABEL_22;
      default:
        break;
    }
    LOWORD(v20) = sub_4E381();
    v13 = sub_17AA9(v20, v14, a3, v15, 1);
  }
  free(v26);
  free(v27);
  return free(v25);
}


// ===== 0x14237 sub_14237 size=918 =====
int __fastcall sub_14237(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6; // ecx
  int v7; // esi
  int v8; // eax
  int v9; // eax
  char *v10; // eax
  char v11; // di
  unsigned __int8 v12; // bp
  int v13; // eax
  int v14; // ebx
  __int64 v15; // rax
  __int32 v16; // eax
  int v17; // edx
  int v18; // ecx
  int v19; // ecx
  unsigned __int8 *v20; // eax
  int v21; // edx
  int v22; // eax
  int v23; // eax
  int v24; // esi
  int v25; // edi
  int v26; // esi
  int v27; // ebp
  int v28; // edi
  _BYTE v30[5]; // [esp-4h] [ebp-64h] BYREF
  unsigned __int8 v31; // [esp+1h] [ebp-5Fh]
  int v32; // [esp+4h] [ebp-5Ch]
  int v33; // [esp+8h] [ebp-58h]
  int v34; // [esp+Ch] [ebp-54h]
  int v35; // [esp+10h] [ebp-50h]
  int v36; // [esp+14h] [ebp-4Ch]
  int v37; // [esp+18h] [ebp-48h]
  _BYTE *v38; // [esp+1Ch] [ebp-44h]
  int v39; // [esp+20h] [ebp-40h]
  int v40; // [esp+24h] [ebp-3Ch]
  int i; // [esp+28h] [ebp-38h]
  int v42; // [esp+2Ch] [ebp-34h]
  int v43; // [esp+30h] [ebp-30h]
  int v44; // [esp+34h] [ebp-2Ch]
  int v45; // [esp+38h] [ebp-28h]
  int v46; // [esp+3Ch] [ebp-24h]
  int v47; // [esp+40h] [ebp-20h]
  int v48; // [esp+44h] [ebp-1Ch]
  int j; // [esp+48h] [ebp-18h]
  int v50; // [esp+4Ch] [ebp-14h]
  int v51; // [esp+5Ch] [ebp-4h]

  sub_3702F(a1, a2, a3, a4, 128);
  v51 = a3;
  v46 = 0;
  v37 = 0;
  v7 = 80 * a5 + dword_53A45;
  v39 = *(unsigned __int16 *)(v7 + 72);
  v40 = *(unsigned __int16 *)(v7 + 74);
  dword_53C4F = 0;
  v8 = sub_1B83D(a5, 0);
  if ( v8 != -1 )
  {
    v9 = sub_1B722(a5, v8);
    v10 = sub_4E8BC(v9);
    v34 = (unsigned __int8)v10[11];
    v35 = (unsigned __int8)v10[12];
    v11 = *(_BYTE *)(v7 + 59);
    v12 = *(_BYTE *)v7;
    v32 = *(unsigned __int8 *)(v7 + 1);
    if ( sub_1F183(a5) != 0 )
      v13 = 19;
    else
      v13 = *(unsigned __int8 *)(v7 + 32);
    v14 = (int)sub_4E8A5(v13);
    v33 = malloc(32);
    v15 = malloc(2048);
    v38 = (_BYTE *)v15;
    if ( a6 == 0 )
      v37 = 1;
    sub_145CD(v15, SHIDWORD(v15), v14, v6, a6);
    sub_4E390(v14, v12, v32, v11, (char *)dword_53A51, dword_53A69);
    v16 = sub_146D1(a5, a6);
    v36 = sub_14B16(v16, v17, v14, v18, v38);
    sub_4DF4C((unsigned __int8 *)dword_53A51);
    v43 = malloc(100);
    for ( i = 0; i < v36; ++i )
    {
      v20 = &v38[2 * i];
      v21 = *v20;
      v47 = v21;
      v48 = v20[1];
      v45 = v39;
      v44 = v40;
      v22 = sub_1F183(a5);
      if ( v22 != 0 )
      {
        sub_12E38(v47, v48, v30);
        v14 = v31;
        v19 = 100;
        v45 = v39 * dword_51A12[v31] / 100 + v39;
        v22 = v40 * dword_51A2A[v31] / 100;
        v21 = v22 + v40;
        v44 = v22 + v40;
      }
      sub_14818(v22, v21, v14, v19, v47, v48, v43, v35, v34, v37);
      v24 = v23;
      v42 = v23;
      sub_4DF4C((unsigned __int8 *)dword_53A51);
      if ( v24 != 0 )
      {
        for ( j = 0; j < v42; ++j )
        {
          v50 = *(unsigned __int8 *)(j + v43);
          v14 = dword_53A45 + 80 * v50;
          v27 = *(unsigned __int16 *)(v14 + 72);
          v28 = *(unsigned __int16 *)(v14 + 74);
          if ( sub_1F183(v50) != 0 )
          {
            sub_12E38(*(unsigned __int8 *)v14, *(unsigned __int8 *)(v14 + 1), v30);
            v19 = 100;
            v27 += v27 * dword_51A12[v31] / 100;
            v28 += v28 * dword_51A2A[v31] / 100;
          }
          v26 = v45 - v28;
          if ( v45 - v28 <= 2 )
            v25 = 0;
          else
            v25 = 8;
          if ( v26 > *(unsigned __int16 *)(v14 + 64) )
          {
            v26 *= 2;
            v25 = 18;
          }
          if ( sub_1DEBE(v50, v47, v48) == 1 )
            v26 += v44 - v27;
          if ( *(_BYTE *)(v14 + 8) == 0 )
            v26 = 3 * v26 / 2;
          if ( v25 > dword_53C4F || v25 == dword_53C4F && v26 > v46 )
          {
            v46 = v26;
            dword_53C43 = v47;
            dword_53C47 = v48;
            dword_53C4B = v50;
            dword_53C4F = v25;
          }
        }
      }
    }
    free(v43);
    free(v33);
    free(v38);
  }
  return 0;
}


// ===== 0x10010 sub_10010 size=1552 =====
void __usercall sub_10010(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // ecx
  int v6; // ebp
  int v7; // eax
  int v8; // ebx
  __int64 v9; // rax
  int v10; // eax
  int v11; // ecx
  __int64 v12; // rax
  int v13; // ecx
  int v14; // ebx
  int i; // ebx
  __int64 v16; // rax
  int v17; // ebx
  int v18; // eax
  int v19; // eax
  int j; // ebx
  int v21; // esi
  int k; // ebx
  int v23; // edi
  int v24; // [esp+0h] [ebp-14h]

  sub_3702F(a1, a2, a4, a3, 60);
  v6 = malloc(22987);
  if ( v6 != 0 )
  {
    v9 = fopen(aFd2Sav_2, aRb_0);
    v8 = v9;
    sub_373CA((_BYTE *)v6, 1u, 22987, v9);
    fclose(v8);
    sub_4DF28(v6, 22987);
    LODWORD(v9) = sub_4DF09(v6, 22987);
    if ( (_DWORD)v9 != *(_DWORD *)(v6 + 22983) )
    {
      LODWORD(v9) = sub_1956B(75);
      sub_15F84(v9, SHIDWORD(v9), v5, v8, a5, dword_53A7D, 436, 696099, 320, 205, 76, 74, 19, 1);
      sub_16559(0);
      v10 = sub_16C57(0);
      LODWORD(v9) = sub_196CB(v10);
    }
    sub_1F882(v9);
    memmove(dword_53BF7, v6 + 2211, 2560);
    dword_53A65 = sub_111BA(aFdotherDat, dword_53A65, 0);
    dword_53C03 = *(unsigned __int8 *)(v6 + 12485);
    v12 = sub_111BA(aFdfieldDat, dword_53A59, 3 * dword_53C03 + 2);
    dword_53A59 = v12;
    if ( dword_53A55 != 0 )
      free(dword_53A55);
    dword_53A55 = malloc(2211);
    if ( dword_53A55 != 0 )
    {
      LODWORD(v12) = memmove(dword_53A55, v6, 2211);
      sub_10652(v12, SHIDWORD(v12), v8, v11);
      dword_53A79 = sub_111BA(aFdtxtDat, dword_53A79, dword_53C03 + 1);
      dword_53A51 = sub_111BA(aFdfieldDat, dword_53A51, 3 * dword_53C03);
      dword_53AC1 = *(__int16 *)dword_53A51;
      dword_53AC5 = *(__int16 *)(dword_53A51 + 2);
      v14 = 2 * *(unsigned __int8 *)dword_53A55;
      dword_53A5D = sub_111BA(aFdshapDat, dword_53A5D, v14);
      dword_53A69 = sub_111BA(aFdshapDat, dword_53A69, v14 + 1);
      sub_4DF4C((unsigned __int8 *)dword_53A51);
      dword_53BE7 = *(unsigned __int8 *)(dword_53A55 + 1);
      dword_53BE3 = *(unsigned __int8 *)(dword_53A55 + 2);
      dword_53BEB = *(unsigned __int8 *)(v6 + 12484);
      if ( dword_53A45 != 0 )
        free(dword_53A45);
      dword_53A45 = malloc(7680);
      if ( dword_53A45 != 0 )
      {
        memmove(dword_53A45, v6 + 4771, 80 * dword_53BEB);
        memmove(dword_53AD5, v6 + 12451, 32);
        if ( dword_53A61 != 0 )
          free(dword_53A61);
        v24 = fopen(aFdiconB24, aRb_1);
        dword_53BDF = 0;
        for ( i = 0; i < dword_53BEB; ++i )
          *(_BYTE *)(80 * i + dword_53A45 + 2) = sub_11019(*(unsigned __int8 *)(80 * i + dword_53A45 + 7), v24);
        fclose(v24);
        v16 = fopen(aFd2Tmp, aWb_0);
        v17 = v16;
        fwrite(dword_53A61, 1, (char *)&loc_329FE + 2, v16);
        fclose(v17);
        dword_53BEF = *(unsigned __int8 *)(v6 + 12483);
        dword_53AA9 = *(unsigned __int8 *)(v6 + 12486);
        dword_53AAD = *(unsigned __int8 *)(v6 + 12487);
        LODWORD(qword_53AB1) = *(unsigned __int8 *)(v6 + 12488);
        HIDWORD(qword_53AB1) = *(unsigned __int8 *)(v6 + 12489);
        dword_53AB9 = *(unsigned __int8 *)(v6 + 12490);
        dword_53ABD = *(unsigned __int8 *)(v6 + 12491);
        dword_53BFB = *(unsigned __int8 *)(v6 + 12492);
        dword_53BF3 = *(_DWORD *)(v6 + 12493);
        byte_53AF9 = *(_BYTE *)(v6 + 12497);
        byte_51AAB = *(_BYTE *)(v6 + 12498);
        byte_51E61 = *(_BYTE *)(v6 + 12499);
        byte_51E62 = *(_BYTE *)(v6 + 12500);
        free(v6);
        free(dword_53A59);
        dword_53A59 = 0;
        v18 = sub_25977(
                (unsigned __int8)byte_51E63[dword_53C03],
                SHIDWORD(v16),
                v17,
                v13,
                (unsigned __int8)byte_51E63[dword_53C03],
                0);
        dword_51A83 = 0;
        sub_12263(v18);
        v19 = sub_11CAC(1);
        sub_1F525(v19);
        for ( j = 0; j < 9; ++j )
        {
          v21 = sub_15F0E(dword_53A81, 655360, 320, 120, 84, j + 83);
          if ( j > 6 )
            sub_187D6(684651, 320, dword_53BEF, 42, 3);
          j___delay(70);
          if ( j == 8 )
            j___delay(500);
          sub_15E71(v21, 655360, 320);
        }
        for ( k = 2; k < 6; ++k )
        {
          if ( k == 5 )
            k = 9;
          v23 = sub_15F0E(dword_53A81, dword_53A49 + 32904, 456, 116, k * k + 84, 91);
          sub_187D6(456 * (k * k + 90) + dword_53A49 + 33071, 456, dword_53BEF, 42, 3);
          sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
          sub_17AA9(1);
          sub_15E71(v23, dword_53A49 + 32904, 456);
        }
        sub_11CAC(0);
        j___delay(200);
        dword_53AE9 = 0;
        dword_51A83 = 1;
        sub_4E381();
        JUMPOUT(0x22BBE);
      }
      word_53A8D = 3;
      int386(16, &word_53A8D, &word_53A8D);
      v7 = printf(aOutOfMemory_1);
    }
    else
    {
      word_53A8D = 3;
      int386(16, &word_53A8D, &word_53A8D);
      v7 = printf(aOutOfMemory_0);
    }
  }
  else
  {
    word_53A8D = 3;
    int386(16, &word_53A8D, &word_53A8D);
    v7 = printf(aOutOfMemory);
  }
  exit(v7);
}

