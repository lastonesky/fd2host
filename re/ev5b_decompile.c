
// ===== 10B4E =====
int __fastcall sub_10B4E(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // esi
  int i; // ebx
  int v7; // ebx

  sub_3702F(a1, a2, a3, a4, 32);
  v5 = fopen(aFdiconB24_0, aRb_11);
  if ( v5 == 0 )
  {
    word_53A8D = 3;
    int386(16, &word_53A8D, &word_53A8D);
    JUMPOUT(0x10056);
  }
  dword_53A59 = sub_111BA(aFdfieldDat, dword_53A59, 3 * dword_53C03 + 2);
  for ( i = 0; i < dword_53BE3; ++i )
  {
    if ( *(unsigned __int8 *)(dword_53A55 + 26 * i + 152) == a5 )
      sub_10C50(i, v5);
  }
  fclose(v5);
  free(dword_53A59);
  dword_53A59 = 0;
  v7 = fopen(aFd2Tmp_0, aWb_1);
  fwrite(dword_53A61, 1, (char *)&loc_329FE + 2, v7);
  return fclose(v7);
}


// ===== 10C50 =====
int __fastcall sub_10C50(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  int v6; // ecx
  int i; // esi
  int j; // ebx
  int v9; // edi
  _BYTE *v10; // eax
  _BYTE *v11; // esi
  char *v12; // eax
  int v13; // edx
  int v14; // ebx
  char *v15; // edi
  char *v16; // eax
  int v17; // edx
  int v18; // ecx
  int k; // eax
  int v20; // ebx
  int result; // eax
  int v22; // [esp+0h] [ebp-44h]
  int v23; // [esp+4h] [ebp-40h]
  unsigned int v24; // [esp+8h] [ebp-3Ch]
  char v25; // [esp+Ch] [ebp-38h]
  __int16 v26; // [esp+10h] [ebp-34h]
  __int16 v27; // [esp+14h] [ebp-30h]
  int v28; // [esp+18h] [ebp-2Ch]
  int v29; // [esp+1Ch] [ebp-28h]
  char v30; // [esp+20h] [ebp-24h]
  int v31; // [esp+24h] [ebp-20h]
  char v32; // [esp+28h] [ebp-1Ch]
  int v33; // [esp+2Ch] [ebp-18h]
  int v34; // [esp+30h] [ebp-14h]

  sub_3702F(a1, a2, a3, a4, 84);
  v34 = 255;
  v29 = 80 * dword_53BEB + dword_53A45;
  v31 = *(unsigned __int8 *)(6 * a5 + dword_53A59 + 2);
  v33 = *(unsigned __int8 *)(6 * a5 + dword_53A59 + 4);
  sub_145CD(0);
  sub_145CD(1);
  if ( byte_53AFA != 0 )
  {
    v30 = v31;
    v32 = v33;
  }
  else
  {
    for ( i = 0; i < dword_53AC5; ++i )
    {
      for ( j = 0; j < dword_53AC1; ++j )
      {
        if ( (*(_BYTE *)(4 * (j + i * dword_53AC1) + dword_53A51 + 6) & 0x40) == 0 )
        {
          v23 = abs(j - v31);
          if ( v23 + abs(i - v33) <= v34 )
          {
            v9 = abs(j - v31);
            v34 = abs(i - v33) + v9;
            v30 = j;
            v32 = i;
          }
        }
      }
    }
  }
  sub_4DF4C((unsigned __int8 *)dword_53A51);
  v10 = (_BYTE *)(dword_53A55 + 26 * a5);
  v11 = v10 + 131;
  v24 = (unsigned __int8)v10[132];
  v28 = (unsigned __int8)v10[135];
  v25 = v10[131];
  if ( v24 < 0x44 )
  {
    v15 = sub_4E838(v24);
    v16 = sub_4E821(v24);
    v27 = (v28 - 1) * (unsigned __int8)v16[6] + *(_WORD *)(v15 + 3);
    v26 = (unsigned __int8)v16[8] * (v28 - 1) + *(_WORD *)(v15 + 5);
    v22 = *((unsigned __int16 *)v15 + 9);
    v17 = *((unsigned __int16 *)v15 + 10);
    v14 = *((unsigned __int16 *)v15 + 11);
    *(_BYTE *)(v29 + 31) = *v15;
    *(_BYTE *)(v29 + 32) = v15[1];
    LOWORD(v6) = (unsigned __int8)*v16;
    v18 = v22 + v28 * v6;
    *(_WORD *)(v29 + 55) = v18;
    LOWORD(v18) = (unsigned __int8)v16[2];
    v6 = v17 + v28 * v18;
    *(_WORD *)(v29 + 57) = v6;
    *(_BYTE *)(v29 + 59) = v15[7];
    LOWORD(v16) = (unsigned __int8)v16[4];
    v12 = (char *)(v14 + v28 * (_DWORD)v16);
    *(_WORD *)(v29 + 62) = (_WORD)v12;
  }
  else
  {
    v12 = sub_4E84F((unsigned __int8)v10[132] - 68);
    v27 = *((_WORD *)v12 + 1) * v28;
    v26 = (unsigned __int8)v12[4] * (_WORD)v28;
    *(_BYTE *)(v29 + 31) = *v12;
    *(_BYTE *)(v29 + 32) = v12[1];
    *(_WORD *)(v29 + 55) = (unsigned __int8)v12[5] * (_WORD)v28;
    HIWORD(v13) = HIWORD(v29);
    *(_WORD *)(v29 + 57) = (unsigned __int8)v12[6] * (_WORD)v28;
    LOWORD(v13) = (unsigned __int8)v12[7];
    v14 = v13 * v28;
    *(_WORD *)(v29 + 62) = v13 * v28;
    *(_BYTE *)(v29 + 59) = v12[8];
  }
  *(_BYTE *)v29 = v30;
  LOBYTE(v12) = v32;
  *(_BYTE *)(v29 + 1) = v32;
  *(_BYTE *)(v29 + 2) = sub_11019((__int32)v12, v29, v14, v6, v24, a6);
  *(_BYTE *)(v29 + 3) = 0;
  *(_BYTE *)(v29 + 4) = 0;
  *(_BYTE *)(v29 + 5) = 0;
  *(_BYTE *)(v29 + 6) = v25;
  *(_BYTE *)(v29 + 7) = v24;
  *(_BYTE *)(v29 + 8) = v24;
  *(_BYTE *)(v29 + 9) = 0;
  if ( (unsigned __int8)v11[5] == 255 )
  {
    *(_BYTE *)(v29 + 10) = 64;
    *(_BYTE *)(v29 + 11) = v11[6];
    *(_BYTE *)(v29 + 12) = 0x80;
  }
  else
  {
    *(_BYTE *)(v29 + 10) = 64;
    *(_BYTE *)(v29 + 11) = v11[5];
    *(_BYTE *)(v29 + 12) = 64;
    *(_BYTE *)(v29 + 13) = v11[6];
  }
  for ( k = 0; k < 6; ++k )
  {
    v20 = v29 + 2 * k;
    if ( (unsigned __int8)v11[k + 7] == 255 )
      *(_BYTE *)(v20 + 14) = 0x80;
    else
      *(_BYTE *)(v20 + 14) = 0;
    *(_BYTE *)(v29 + 2 * k + 15) = v11[k + 7];
  }
  memset(v29 + 34, 0, 6);
  memmove(v29 + 26, v11 + 13, 4);
  *(_BYTE *)(v29 + 30) = 0;
  *(_BYTE *)(v29 + 33) = v28;
  *(_BYTE *)(v29 + 49) = v11[22];
  *(_WORD *)(v29 + 50) = *(_WORD *)(v11 + 23);
  *(_BYTE *)(v29 + 52) = v11[17];
  *(_BYTE *)(v29 + 53) = v11[18];
  *(_BYTE *)(v29 + 54) = v11[19];
  *(_BYTE *)(v29 + 61) = v11[2];
  if ( *(_BYTE *)(v29 + 6) == 2 )
    *(_BYTE *)(v29 + 60) = 0;
  else
    *(_BYTE *)(v29 + 60) = -1;
  *(_WORD *)(v29 + 64) = v27;
  *(_WORD *)(v29 + 66) = v27;
  *(_WORD *)(v29 + 68) = v26;
  *(_WORD *)(v29 + 70) = v26;
  result = sub_1B750(dword_53BEB);
  ++dword_53BEB;
  return result;
}


// ===== 11019 =====
int __fastcall sub_11019(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  _BYTE *v6; // ebp
  int i; // eax
  int m; // eax
  int j; // eax
  int k; // eax
  _DWORD v13[13]; // [esp+0h] [ebp-48h]
  int v14; // [esp+34h] [ebp-14h]
  int v15; // [esp+44h] [ebp-4h]

  sub_3702F(a1, a2, a3, a4, 92);
  v15 = a3;
  fseek(a6, 6, 0);
  v6 = (_BYTE *)malloc(6720);
  sub_373CA(v6, 1u, 6720, a6);
  for ( i = 0; i < 13; ++i )
    v13[i] = *(_DWORD *)&v6[48 * a5 + 4 * i];
  v14 = v13[12] - v13[0];
  free(v6);
  if ( dword_53BDF != 0 )
  {
    for ( j = 0; j < dword_53BDF; ++j )
    {
      if ( a5 == dword_53B17[j] )
        return j;
    }
    dword_53B17[j] = a5;
    fseek(a6, v13[0], 0);
    sub_373CA((_BYTE *)(dword_539EC + dword_53A61), 1u, v14, a6);
    for ( k = 0; k < 12; ++k )
      *(_DWORD *)(dword_53A61 + 4 * (k + 12 * dword_53BDF)) = v13[k] - v13[0] + dword_539EC;
    dword_539EC += v14;
    return dword_53BDF++;
  }
  else
  {
    dword_53B17[0] = a5;
    dword_53A61 = malloc((char *)&loc_329FE + 2);
    fseek(a6, v13[0], 0);
    sub_373CA((_BYTE *)(dword_53A61 + 1920), 1u, v14, a6);
    for ( m = 0; m < 12; ++m )
      *(_DWORD *)(dword_53A61 + 4 * m) = v13[m] - v13[0] + 1920;
    ++dword_53BDF;
    dword_539EC = v14 + 1920;
    return 0;
  }
}


// ===== 145CD =====
void __fastcall sub_145CD(__int32 a1, int a2, int a3, int a4, int a5)
{
  unsigned __int8 *v5; // ebx
  int i; // esi

  sub_3702F(a1, a2, a3, a4, 24);
  v5 = (unsigned __int8 *)dword_53A45;
  for ( i = 0; i < dword_53BEB; ++i )
  {
    if ( (v5[5] & 1) == 0 && (a5 == 0 && v5[6] != 0 || a5 != 0 && v5[6] == 0) )
      sub_14625(*v5, v5[1]);
    v5 += 80;
  }
  JUMPOUT(0x145C9);
}


// ===== 14625 =====
_BYTE *__fastcall sub_14625(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  _BYTE *result; // eax

  sub_3702F(a1, a2, a3, a4, 20);
  if ( a5 != 0 )
    sub_146A7(a5 - 1, a6);
  if ( a6 != 0 )
    sub_146A7(a5, a6 - 1);
  if ( a5 < dword_53AC1 - 1 )
    sub_146A7(a5 + 1, a6);
  if ( a6 < dword_53AC5 - 1 )
    sub_146A7(a5, a6 + 1);
  result = (_BYTE *)(dword_53A51 + 4 * (a5 + dword_53AC1 * a6) + 6);
  *result |= 0x40u;
  return result;
}


// ===== 146A7 =====
_BYTE *__fastcall sub_146A7(__int32 a1, int a2, int a3, int a4, int a5, int a6)
{
  _BYTE *result; // eax

  sub_3702F(a1, a2, a3, a4, 4);
  result = (_BYTE *)(4 * (a5 + dword_53AC1 * a6) + dword_53A51 + 6);
  *result |= 0x80u;
  return result;
}


// ===== 1B750 =====
void __fastcall sub_1B750(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // edi
  int i; // esi
  char *v7; // eax
  double v8; // st7
  double v9; // st7
  __int16 v10; // [esp+0h] [ebp-1Ch]
  int v11; // [esp+4h] [ebp-18h]
  int v12; // [esp+8h] [ebp-14h]
  int v13; // [esp+Ch] [ebp-10h]

  sub_3702F(a1, a2, a3, a4, 36);
  v5 = 80 * a5 + dword_53A45;
  v12 = *(__int16 *)(v5 + 55);
  v13 = *(__int16 *)(v5 + 57);
  v11 = *(__int16 *)(v5 + 62);
  if ( *(_BYTE *)(v5 + 36) != 0 )
    v11 += 15;
  v10 = v11;
  for ( i = 0; i < 8; ++i )
  {
    if ( (*(_BYTE *)(v5 + 2 * i + 10) & 0x40) != 0 )
    {
      v7 = sub_4E8BC(*(unsigned __int8 *)(v5 + 2 * i + 11));
      v12 += *(__int16 *)(v7 + 1);
      v13 += *(__int16 *)(v7 + 5);
      v10 += *(_WORD *)(v7 + 3);
      v11 += *(__int16 *)(v7 + 7);
    }
  }
  if ( *(_BYTE *)(v5 + 34) != 0 )
  {
    v8 = (double)v12 * dbl_5018D;
    _CHP();
    v12 = (int)v8;
  }
  if ( *(_BYTE *)(v5 + 35) != 0 )
  {
    v9 = (double)v13 * dbl_5018D;
    _CHP();
    v13 = (int)v9;
  }
  *(_WORD *)(v5 + 72) = v12;
  *(_WORD *)(v5 + 74) = v13;
  *(_WORD *)(v5 + 76) = v10;
  JUMPOUT(0x114FB);
}


// ===== 32999 =====
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
