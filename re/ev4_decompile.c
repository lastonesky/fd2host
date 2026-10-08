
// ===== 1366A =====
int __fastcall sub_1366A(__int32 a1, int a2, int a3, int a4, int a5)
{
  unsigned __int8 *v5; // eax
  unsigned __int8 *v6; // ebp
  int v7; // eax
  int v8; // eax
  int v9; // edi
  int m; // esi
  int v11; // eax
  int v12; // ebx
  _BYTE *v13; // eax
  int v14; // edx
  _BYTE v16[32]; // [esp+0h] [ebp-6Ch]
  _BYTE v17[32]; // [esp+20h] [ebp-4Ch]
  unsigned __int8 i; // [esp+40h] [ebp-2Ch]
  unsigned __int8 v19; // [esp+44h] [ebp-28h]
  unsigned __int8 v20; // [esp+48h] [ebp-24h]
  unsigned __int8 k; // [esp+4Ch] [ebp-20h]
  unsigned __int8 j; // [esp+50h] [ebp-1Ch]
  unsigned __int8 v23; // [esp+54h] [ebp-18h]
  unsigned __int8 v24; // [esp+58h] [ebp-14h]
  int v25; // [esp+68h] [ebp-4h]

  sub_3702F(a1, a2, a3, a4, 136);
  v25 = a3;
  v5 = (unsigned __int8 *)sub_4EB48(a5);
  v20 = *v5;
  v24 = 0;
  v6 = v5 + 1;
  while ( v24 < (unsigned int)v20 )
  {
    v23 = *v6;
    v19 = v6[1];
    k = 0;
    for ( v6 += 2; ; v6 += 2 )
    {
      v7 = k;
      if ( k >= (unsigned int)v19 )
        break;
      v17[k] = *v6;
      v16[v7] = v6[1];
      ++k;
    }
    if ( (v23 & 0x80u) == 0 )
    {
      for ( i = 0; i < (unsigned int)v23; ++i )
      {
        for ( j = 1; j < 7u; ++j )
        {
          sub_32230(v17[0]);
          for ( k = 0; k < (unsigned int)v19; ++k )
          {
            v11 = 80 * (unsigned __int8)v17[k];
            v12 = dword_53A45;
            *(_BYTE *)(dword_53A45 + v11 + 3) = v16[k];
            *(_BYTE *)(v12 + v11 + 4) = j;
          }
          if ( dword_53AFB == 0 || dword_53AFB == 64 )
          {
            sub_11CAC(0);
          }
          else
          {
            ++dword_53AFB;
            sub_11CAC(1);
            sub_11D40(0, 255, dword_53AFB);
          }
          sub_17AA9(1);
          sub_4E381();
        }
        for ( k = 0; k < (unsigned int)v19; ++k )
        {
          v13 = (_BYTE *)(dword_53A45 + 80 * (unsigned __int8)v17[k]);
          v14 = (unsigned __int8)v16[k];
          if ( v16[k] != 0 )
          {
            if ( v14 == 1 )
            {
              --*v13;
            }
            else if ( v14 == 3 )
            {
              ++*v13;
            }
            else
            {
              --v13[1];
            }
          }
          else
          {
            ++v13[1];
          }
          v13[4] = 0;
        }
      }
    }
    else
    {
      v23 &= ~0x80u;
      if ( v23 != 0 )
      {
        for ( k = 0; k < (unsigned int)v19; ++k )
          *(_BYTE *)(dword_53A45 + 80 * (unsigned __int8)v17[k] + 3) = v16[k];
        for ( k = 0; k < (unsigned int)v23; ++k )
        {
          sub_11CAC(0);
          sub_17AA9(1);
          sub_4E381();
        }
      }
      else
      {
        sub_17AA9(1);
        v8 = sub_11EEE(dword_53A49 + 32904, 456, 13, 8, dword_53AA9, dword_53AAD);
        v9 = dword_53A49;
        for ( m = 0; m < dword_53BEB; ++m )
        {
          v8 = 80 * m + dword_53A45;
          for ( k = 0; k < (unsigned int)v19; ++k )
          {
            if ( m == (unsigned __int8)v17[k] )
            {
              dword_53A49 = v9 - 5472;
              *(_BYTE *)(v8 + 3) = v16[k];
            }
          }
          if ( (*(_BYTE *)(v8 + 5) & 1) == 0 )
            v8 = sub_127E0(m);
          dword_53A49 = v9;
        }
        sub_129EC(v8);
        sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
        sub_17AA9(2);
        sub_11CAC(0);
        sub_4E381();
      }
    }
    ++v24;
  }
  return sub_11CAC(1);
}


// ===== 196CB =====
int __fastcall sub_196CB(__int32 a1, int a2, int a3, int a4)
{
  int v4; // edx
  int v5; // ecx
  int i; // ebx
  __int32 v7; // eax

  sub_3702F(a1, a2, a3, a4, 20);
  for ( i = 1; i < 6; ++i )
    sub_1974C(13 * i + 112, dword_53C5B, dword_53C63);
  memmove(655360, dword_53C5F, 64000);
  free(dword_53C5B);
  free(dword_53C5F);
  v7 = free(dword_53C63);
  return sub_11CAC(v7, v4, i, v5, 0);
}


// ===== 197E5 =====
void __fastcall sub_197E5(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  char *v5; // edi
  __int32 v6; // eax
  int v7; // edx
  int v8; // ecx
  int i; // ebp
  int v10; // esi
  int j; // esi
  int k; // esi
  _DWORD v13[2]; // [esp+0h] [ebp-20h]
  int v14; // [esp+8h] [ebp-18h]
  int v15; // [esp+Ch] [ebp-14h]
  int v16; // [esp+1Ch] [ebp-4h]

  v4 = sub_3702F(a1, a2, a3, a4, 60);
  v16 = a3;
  v13[0] = unk_51EE5;
  v13[1] = unk_51EE9;
  v5 = (char *)&loc_1A599 + dword_53A49 + 3;
  v14 = -16;
  v15 = 16;
  if ( (unsigned int)dword_53A51 > 1 )
  {
    sub_1297D(v4);
    v6 = sub_11EEE(dword_53A49 + 32904, 456, 13, 8, dword_53AA9, dword_53AAD);
    sub_127A9(v6, v7, a3, v8);
  }
  for ( i = 0; i < 4; ++i )
  {
    v10 = 0;
    v14 += 4;
    v15 -= 4;
    while ( v10 < 86 )
    {
      memmove(dword_53A49 + 32905 + 456 * (v10 + 108), dword_53C63 + 35845 + 320 * v10, 310);
      ++v10;
    }
    for ( j = 0; j < 2; ++j )
      sub_4ED34((int)&v5[*(&v14 + j)], dword_53A89 + *(_DWORD *)(12 * v13[j] + dword_53A89), 456);
    sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
  }
  for ( k = 0; k < 86; ++k )
    memmove(320 * k + 691205, 320 * (k + 112) + dword_53C63 + 5, 310);
  JUMPOUT(0x17E03);
}


// ===== 19953 =====
void __fastcall sub_19953(__int32 a1, int a2, int a3, int a4)
{
  int v4; // ecx
  int v5; // eax
  int v6; // ebx
  int v7; // edx
  int v8; // ebp
  char *v9; // edi
  unsigned int v10; // eax
  int i; // esi
  __int32 v12; // eax
  int v13; // edx
  int v14; // ecx
  int v15; // esi
  int k; // esi
  __int32 v17; // eax
  int v18; // edx
  int v19; // ecx
  int v20; // edx
  int v21; // eax
  int v22; // eax
  __int16 *v23; // eax
  int n; // esi
  int m; // esi
  int ii; // esi
  int v27; // eax
  int v28; // ebx
  _DWORD v29[2]; // [esp+0h] [ebp-28h]
  int v30; // [esp+8h] [ebp-20h]
  int v31; // [esp+Ch] [ebp-1Ch]
  int j; // [esp+10h] [ebp-18h]
  char v33; // [esp+14h] [ebp-14h]
  int v34; // [esp+24h] [ebp-4h]

  sub_3702F(a1, a2, a3, a4, 68);
  v34 = a3;
  v29[0] = unk_51EED;
  v29[1] = unk_51EF1;
  v33 = 0;
  dword_53C57 = 0;
  LOWORD(v5) = sub_4EBE3();
  v6 = 30;
  v7 = v5 % 30;
  v8 = v5 % 30 + 2;
  v9 = (char *)&loc_1A599 + dword_53A49 + 3;
  v10 = memmove(dword_53C63, 655360, 64000);
  v30 = 0;
  v31 = 0;
  for ( i = 0; i < 200; ++i )
  {
    v7 = dword_53A49 + 32900;
    v10 = memmove(dword_53A49 + 32900 + 456 * (i - 4), dword_53C63 + 320 * i, 320);
  }
  if ( (unsigned int)dword_53A51 > 1 )
  {
    sub_1297D(v10);
    v12 = sub_11EEE(dword_53A49 + 32904, 456, 13, 8, dword_53AA9, dword_53AAD);
    v10 = sub_127A9(v12, v13, 30, v14);
  }
  for ( j = 0; j < 4; ++j )
  {
    v15 = 0;
    v30 -= 4;
    v31 += 4;
    while ( v15 < 86 )
    {
      v6 = dword_53C63;
      v7 = 456 * (v15 + 108);
      memmove(v7 + dword_53A49 + 32905, 320 * v15++ + dword_53C63 + 35845, 310);
    }
    for ( k = 0; k < 2; ++k )
    {
      v6 = dword_53A89;
      v7 = dword_53A89 + *(_DWORD *)(12 * v29[k] + dword_53A89);
      sub_4ED34((int)&v9[*(&v30 + k)], v7, 456);
    }
    v10 = sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
  }
  while ( 1 )
  {
    while ( !sub_10620(v10, v7, v6, v4) )
    {
      sub_4E31C();
      v10 = MEMORY[0x46C] - dword_53C17;
      if ( v10 >= 2 )
      {
        if ( ++dword_53C13 == 4 )
          dword_53C13 = 0;
        dword_53C17 = MEMORY[0x46C];
        if ( (unsigned int)dword_53A51 > 1 )
        {
          sub_1297D(MEMORY[0x46C]);
          v17 = sub_11EEE(dword_53A49 + 32904, 456, 13, 8, dword_53AA9, dword_53AAD);
          sub_127A9(v17, v18, v6, v19);
        }
        if ( v33 != 0 )
        {
          v20 = *(unsigned __int8 *)dword_53A85;
          if ( dword_53A51 != 0 )
            sub_4EC31((_BYTE *)(dword_53C67 + dword_53C63), (__int16 *)(v20 + dword_53A85), 320);
          else
            sub_4EBFF((_BYTE *)(dword_53C67 + dword_53C63), (__int16 *)(v20 + dword_53A85), 320);
          LOWORD(v21) = sub_4EBE3();
          v6 = 30;
          v7 = v21 % 30;
          v8 = v21 % 30 + 10;
          v33 = 0;
        }
        else
        {
          v22 = v8--;
          if ( v22 == 0 )
          {
            v23 = (__int16 *)(*(_DWORD *)(dword_53A85 + 12) + dword_53A85);
            if ( dword_53A51 != 0 )
              sub_4EC31((_BYTE *)(dword_53C67 + dword_53C63), v23, 320);
            else
              sub_4EBFF((_BYTE *)(dword_53C67 + dword_53C63), v23, 320);
            v33 = 1;
          }
        }
        if ( (unsigned int)dword_53A51 <= 1 )
        {
          for ( m = 0; m < 200; ++m )
          {
            v7 = 456 * (m - 4);
            memmove(v7 + dword_53A49 + 32900, 320 * m + dword_53C63, 320);
          }
        }
        else
        {
          for ( n = 0; n < 86; ++n )
          {
            v7 = 456 * (n + 108);
            memmove(v7 + dword_53A49 + 32905, dword_53C63 + 35845 + 320 * n, 310);
          }
        }
        for ( ii = 0; ii < 2; ++ii )
        {
          v28 = 3 * v29[ii];
          if ( ii == dword_53C57 )
          {
            v7 = dword_53C13 >> 31;
            v28 += dword_53C13 / 2;
          }
          v27 = v28;
          v6 = dword_53A89;
          sub_4ED34((int)&v9[*(&v30 + ii)], dword_53A89 + *(_DWORD *)(dword_53A89 + 4 * v27), 456);
        }
        v10 = sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
      }
    }
    HIBYTE(word_53A8D) = 16;
    int386(22, &word_53A8D, &word_53A8D);
    v10 = HIBYTE(word_53A8D);
    if ( HIBYTE(word_53A8D) == 224 || HIBYTE(word_53A8D) == 82 || HIBYTE(word_53A8D) == 28 || HIBYTE(word_53A8D) == 57 )
      break;
    switch ( HIBYTE(word_53A8D) )
    {
      case 1u:
      case 0x53u:
        dword_53C13 = 0;
LABEL_57:
        JUMPOUT(0x13FCC);
      case 0x4Bu:
        dword_53C57 = 0;
        break;
      case 0x4Du:
        dword_53C57 = 1;
        break;
      default:
        break;
    }
  }
  dword_53C13 = 0;
  goto LABEL_57;
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


// ===== 1DB65 =====
int __fastcall sub_1DB65(__int32 a1, int a2, int a3, int a4)
{
  int result; // eax
  int v5; // ecx
  int v6; // ebp
  int i; // esi
  int v8; // edi
  int mm; // esi
  int v10; // ebx
  int j; // edi
  int v12; // edx
  __int32 v13; // eax
  int v14; // ecx
  int v15; // eax
  int k; // esi
  int m; // esi
  int v18; // ebx
  int v19; // ebx
  __int32 v20; // eax
  int v21; // esi
  int v22; // eax
  int v23; // edx
  int v24; // ecx
  int v25; // edx
  int v26; // ecx
  int n; // edi
  int ii; // esi
  int v29; // edx
  __int32 v30; // eax
  int jj; // edi
  int kk; // esi
  int v33; // edx
  __int32 v34; // eax
  __int32 v35; // eax
  _DWORD v36[30]; // [esp+0h] [ebp-8Ch]
  int v37; // [esp+78h] [ebp-14h]
  int v38; // [esp+88h] [ebp-4h]

  result = sub_3702F(a1, a2, a3, a4, 168);
  v38 = a3;
  v6 = 0;
  for ( i = 0; i < dword_53BEB; ++i )
  {
    a3 = 80 * i + dword_53A45;
    v5 = *(unsigned __int8 *)a3;
    v8 = *(unsigned __int8 *)(a3 + 1);
    if ( (*(_BYTE *)(a3 + 5) & 1) == 0
      && *(_WORD *)(a3 + 64) == 0
      && v5 >= dword_53AA9 - 1
      && v5 <= dword_51A87 + dword_53AA9
      && v8 >= dword_53AAD - 1
      && v8 <= dword_51A8B + dword_53AAD + 1 )
    {
      a3 = 1216 * (v8 - 1 - dword_53AAD);
      v5 = 10944 * (v8 - 1 - dword_53AAD) + dword_53A49 + 32904 + 24 * (v5 - 1 - dword_53AA9) - 2736;
      v36[v6++] = v5;
    }
  }
  if ( v6 != 0 )
  {
    for ( j = 0; j < 13; ++j )
    {
      v15 = sub_11EEE(dword_53A49 + 32904, 456, 13, 8, dword_53AA9, dword_53AAD);
      for ( k = 0; k < dword_53BEB; ++k )
      {
        a3 = dword_53A45 + 80 * k;
        if ( (*(_BYTE *)(a3 + 5) & 1) == 0 )
        {
          if ( *(_WORD *)(a3 + 64) == 0 )
            *(_BYTE *)(a3 + 3) = j % 4;
          v15 = sub_127E0(k);
        }
      }
      sub_129EC(v15);
      v12 = dword_53A49 + 32904;
      v13 = sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
      sub_17AA9(v13, v12, a3, v14, 1);
    }
    for ( m = 0; m < dword_53BEB; ++m )
    {
      v18 = 80 * m + dword_53A45;
      if ( *(_WORD *)(v18 + 64) == 0 )
        *(_BYTE *)(v18 + 5) = 1;
    }
    v19 = malloc((char *)&loc_2567F + 1);
    v37 = v19;
    v20 = sub_11EEE(v19 + 32904, 456, 13, 8, dword_53AA9, dword_53AAD);
    v21 = dword_53A49;
    dword_53A49 = v19;
    v22 = sub_127A9(v20, v19 + 32904, v19, v5);
    dword_53A49 = v21;
    sub_25A96(v22, v23, v19, v24, dword_53EEC, 3, 1);
    for ( n = 0; n < 6; ++n )
    {
      for ( ii = 0; ii < v6; ++ii )
      {
        v19 = n + 68;
        sub_4EBAB((_BYTE *)v36[ii], (__int16 *)(*(_DWORD *)(dword_53A81 + 4 * (n + 68) + 6) + dword_53A81), 456);
      }
      v29 = dword_53A49 + 32904;
      v30 = sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
      sub_17AA9(v30, v29, v19, v26, 1);
    }
    for ( jj = 6; jj < 12; ++jj )
    {
      memmove(dword_53A49, v37, (char *)&loc_2567F + 1);
      for ( kk = 0; kk < v6; ++kk )
      {
        v19 = jj + 68;
        sub_4EBAB((_BYTE *)v36[kk], (__int16 *)(*(_DWORD *)(dword_53A81 + 4 * (jj + 68) + 6) + dword_53A81), 456);
      }
      v33 = dword_53A49 + 32904;
      v34 = sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
      sub_17AA9(v34, v33, v19, v26, 1);
    }
    v35 = free(v37);
    return sub_11CAC(v35, v25, v19, v26, 0);
  }
  else
  {
    for ( mm = 0; mm < dword_53BEB; ++mm )
    {
      v10 = 80 * mm + dword_53A45;
      if ( *(_WORD *)(v10 + 64) == 0 )
        *(_BYTE *)(v10 + 5) = 1;
    }
  }
  return result;
}


// ===== 11AA8 =====
int __fastcall sub_11AA8(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int v5; // edx
  int v6; // ecx

  v4 = sub_3702F(a1, a2, a3, a4, 20);
  while ( !sub_10620(v4, v5, a3, v6) )
  {
    sub_4E31C();
    word_539F0 = MEMORY[0x46C];
    v5 = MEMORY[0x46C];
    v4 = word_539F2;
    if ( MEMORY[0x46C] != word_539F2 )
    {
      sub_11CAC(word_539F2, MEMORY[0x46C], a3, v6, 0);
      v4 = MEMORY[0x46C];
      word_539F2 = MEMORY[0x46C];
    }
  }
  HIBYTE(word_53A8D) = 16;
  int386(22, &word_53A8D, &word_53A8D);
  if ( HIBYTE(word_53A8D) == 224 || HIBYTE(word_53A8D) == 82 )
    HIBYTE(word_53A8D) = 28;
  if ( HIBYTE(word_53A8D) == 83 )
    HIBYTE(word_53A8D) = 1;
  return HIBYTE(word_53A8D);
}


// ===== 11B48 =====
__int32 __fastcall sub_11B48(__int32 a1, int a2, int a3, int a4)
{
  __int32 result; // eax
  int v5; // edx
  int v6; // ecx

  result = sub_3702F(a1, a2, a3, a4, 8);
  if ( HIDWORD(qword_53AB1) != 0 )
  {
    if ( dword_53ABD < 2 && dword_53AAD != 0 )
    {
      --HIDWORD(qword_53AB1);
      --dword_53AAD;
    }
    else
    {
      --HIDWORD(qword_53AB1);
      --dword_53ABD;
      if ( dword_51A83 == 0 )
        return result;
    }
  }
  return sub_11CAC(result, v5, a3, v6, 0);
}


// ===== 11B9B =====
int __fastcall sub_11B9B(__int32 a1, int a2, int a3, int a4)
{
  int v4; // edx
  int v5; // ecx
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 8);
  result = dword_53AC5 - 1;
  if ( dword_53AC5 - 1 != HIDWORD(qword_53AB1) )
  {
    if ( dword_53ABD <= 5 || (result = dword_53AC5 - 8, dword_53AC5 - 8 == dword_53AAD) )
    {
      ++HIDWORD(qword_53AB1);
      ++dword_53ABD;
      if ( dword_51A83 == 0 )
        return result;
    }
    else
    {
      ++HIDWORD(qword_53AB1);
      ++dword_53AAD;
    }
  }
  return sub_11CAC(result, v4, a3, v5, 0);
}


// ===== 11BFA =====
int __fastcall sub_11BFA(__int32 a1, int a2, int a3, int a4)
{
  int result; // eax

  sub_3702F(a1, a2, a3, a4, 8);
  result = dword_53AC1 - 1;
  if ( dword_53AC1 - 1 != (_DWORD)qword_53AB1 )
  {
    if ( dword_53AB9 <= 10 || (result = dword_53AC1 - 13, dword_53AC1 - 13 == dword_53AA9) )
    {
      LODWORD(qword_53AB1) = qword_53AB1 + 1;
      ++dword_53AB9;
      if ( dword_51A83 == 0 )
        return result;
    }
    else
    {
      LODWORD(qword_53AB1) = qword_53AB1 + 1;
      ++dword_53AA9;
    }
  }
  return sub_11CAC(0);
}


// ===== 11C59 =====
int __fastcall sub_11C59(__int32 a1, int a2, int a3, int a4)
{
  int result; // eax

  result = sub_3702F(a1, a2, a3, a4, 8);
  if ( (_DWORD)qword_53AB1 != 0 )
  {
    if ( dword_53AB9 < 2 && dword_53AA9 != 0 )
    {
      LODWORD(qword_53AB1) = qword_53AB1 - 1;
      --dword_53AA9;
    }
    else
    {
      LODWORD(qword_53AB1) = qword_53AB1 - 1;
      --dword_53AB9;
      if ( dword_51A83 == 0 )
        return result;
    }
  }
  return sub_11CAC(0);
}


// ===== 12263 =====
char __fastcall sub_12263(__int32 a1, int a2, int a3, int a4)
{
  int v4; // eax
  int i; // esi
  int j; // ebx
  _WORD v8[2]; // [esp-2h] [ebp-10h] BYREF
  char v9; // [esp+2h] [ebp-Ch]
  int v10; // [esp+Ah] [ebp-4h]

  LOBYTE(v4) = sub_3702F(a1, a2, a3, a4, 32);
  v10 = a3;
  for ( i = 0; i < dword_53AC5; ++i )
  {
    for ( j = 0; j < dword_53AC1; ++j )
    {
      sub_12E38(j, i, v8);
      LOBYTE(v4) = v9 & 0x60;
      if ( (v9 & 0x60) == 0x20 )
      {
        LOBYTE(v4) = *(_BYTE *)(v8[1] + dword_53AD5);
        if ( (_BYTE)v4 != 0 )
        {
          v4 = dword_53A51 + 4 * (j + i * dword_53AC1) + 4;
          ++*(_WORD *)v4;
          *(_BYTE *)(v4 + 2) = 0;
        }
      }
    }
  }
  return v4;
}


// ===== 15F0E =====
void __fastcall sub_15F0E(__int32 a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
  __int16 *v10; // esi
  int v11; // ebx
  int v12; // ebp
  int v13; // eax
  int v14; // [esp+0h] [ebp-14h]

  sub_3702F(a1, a2, a3, a4, 48);
  v10 = (__int16 *)(*(_DWORD *)(a5 + 4 * a10 + 6) + a5);
  v14 = *v10;
  v11 = v10[1];
  v12 = a8 + a7 * a9;
  v13 = malloc(v11 * v14 + 8);
  sub_4ECBF(v13, v14, v11, a6, v12);
  sub_4EBAB((_BYTE *)(a6 + v12), v10, a7);
  JUMPOUT(0x15983);
}


// ===== 14818 =====
void __fastcall sub_14818(__int32 a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
  char *v10; // eax
  _BYTE *v11; // ebx
  int j; // ebp
  int k; // esi
  int v14; // edi
  int v15; // esi
  int v16; // ebx
  int i; // ebp
  int m; // ebx
  unsigned __int8 *v19; // eax
  char *v20; // [esp-8h] [ebp-1Ch]
  int v21; // [esp-4h] [ebp-18h]
  int v22; // [esp+0h] [ebp-14h]

  sub_3702F(a1, a2, a3, a4, 48);
  v22 = 0;
  if ( a8 >= 16 )
  {
    v15 = 0;
    v16 = a8 - 16;
    while ( v15 < dword_53AC1 )
    {
      if ( abs(v15 - a5) <= v16 )
        *(_BYTE *)(dword_53A51 + 4 * (v15 + dword_53AC1 * a6) + 7) = 0;
      ++v15;
    }
    for ( i = 0; i < dword_53AC5; ++i )
    {
      if ( abs(i - a6) <= v16 )
        *(_BYTE *)(4 * (a5 + i * dword_53AC1) + dword_53A51 + 7) = 0;
    }
  }
  else
  {
    v21 = dword_53A69;
    v20 = (char *)dword_53A51;
    v10 = sub_4E8A5(0);
    sub_4E390((int)v10, a5, a6, a8, v20, v21);
    if ( a9 != 0 )
    {
      v11 = (_BYTE *)(dword_53A51 + 7);
      for ( j = 0; j < dword_53AC5; ++j )
      {
        for ( k = 0; k < dword_53AC1; ++k )
        {
          v14 = abs(k - a5);
          if ( v14 + abs(j - a6) < a9 )
            *v11 = -1;
          v11 += 4;
        }
      }
    }
  }
  for ( m = 0; m < dword_53BEB; ++m )
  {
    v19 = (unsigned __int8 *)(80 * m + dword_53A45);
    if ( (v19[5] & 1) == 0
      && *(unsigned __int8 *)(4 * (dword_53AC1 * v19[1] + *v19) + dword_53A51 + 7) != 255
      && (a10 == 0 && v19[6] == 0 || a10 == 1 && v19[6] != 0 || a10 == 2 && v19[6] == 1 || a10 == 3 && v19[6] == 2) )
    {
      if ( a7 != 0 )
        *(_BYTE *)(v22 + a7) = m;
      ++v22;
    }
  }
  JUMPOUT(0x22BBE);
}
