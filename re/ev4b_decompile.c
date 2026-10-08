
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


// ===== 1E0DB =====
void __fastcall sub_1E0DB(__int32 a1, int a2, int a3, int a4, int a5, char a6, int a7)
{
  unsigned int v7; // edi
  int v8; // esi
  unsigned __int8 *v9; // eax
  int v10; // edx
  int v11; // ebx
  int i; // ebx
  int v13; // eax
  unsigned int v14; // edx
  int v15; // eax
  _DWORD v16[6]; // [esp+0h] [ebp-18h] BYREF

  sub_3702F(a1, a2, a3, a4, 40);
  v16[5] = a3;
  strcpy((char *)v16, "    ");
  v7 = 3;
  v8 = 0;
  v9 = (unsigned __int8 *)(80 * a7 + dword_53A45);
  v10 = *v9;
  v11 = v9[1];
  if ( v10 > dword_53AA9 - 1
    && v10 < dword_51A87 + dword_53AA9
    && v11 >= dword_53AAD - 1
    && v11 <= dword_51A8B + dword_53AAD )
  {
    for ( i = 0; i < 4; ++i )
    {
      sprintf(v16, "%d", a5);
      v13 = dword_53EC4;
      byte_53D34[i + dword_53EC4] = 5 * i + 2;
      byte_53DFC[i + v13] = a7;
      v14 = strlen(v16);
      v15 = i + dword_53EC4;
      if ( v14 <= v7 )
        byte_53C6C[v15] = 0;
      else
        byte_53C6C[v15] = *((_BYTE *)v16 + v8++) + a6 - 48;
      --v7;
    }
    dword_53EC4 += 4;
  }
  JUMPOUT(0x10B46);
}


// ===== 24B4D =====
int __fastcall sub_24B4D(__int32 a1, int a2, int a3, int a4, int a5)
{
  __int32 v5; // eax
  int v6; // edx
  int v7; // ecx
  int result; // eax
  int i; // ebx

  sub_3702F(a1, a2, a3, a4, 36);
  v5 = sub_11EEE(dword_53A49 + 32904, 456, 13, 9, dword_53AA9, dword_53AAD);
  result = sub_11CAC(v5, v6, a3, v7, 0);
  for ( i = 0; i < a5; ++i )
  {
    sub_11EB0(656644, 320, 456 * (i & 1) + dword_53A49 + 32904, 456, 312, 192);
    result = j___delay(20);
  }
  return result;
}


// ===== 31BDF =====
__int16 __usercall sub_31BDF@<ax>(
        __int32 a1@<eax>,
        int a2@<edx>,
        int a3@<ecx>,
        int a4@<ebx>,
        unsigned __int8 *a5@<edi>,
        int a6,
        int a7)
{
  __int32 v7; // eax
  int v8; // edx
  int v9; // ecx
  __int32 v10; // eax
  int v11; // edx
  int v12; // ecx
  __int16 *v13; // eax
  int v14; // edx
  int v15; // ecx
  int v16; // eax

  sub_3702F(a1, a2, a4, a3, 44);
  sub_4E381();
  sub_1956B(a6);
  LOWORD(v7) = sub_4E381();
  sub_15F84(v7, v8, v9, a4, a5, dword_53A79, a7, 693524, 320, 205, 76, 74, 19, 1);
  v13 = sub_16559(v10, v11, a4, v12, 0);
  sub_16C57((__int32)v13, v14, a4, v15, 0);
  sub_26996(v16);
  return sub_4E381();
}


// ===== 1E1DC =====
int __fastcall sub_1E1DC(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // ebx
  int v6; // edx
  int result; // eax
  int i; // edx
  char v9; // al
  char v10; // al
  _DWORD v11[2]; // [esp+0h] [ebp-8h]

  sub_3702F(a1, a2, a3, a4, 12);
  v11[1] = a3;
  v11[0] = dword_5204A;
  v5 = *(unsigned __int8 *)(80 * a5 + dword_53A45);
  v6 = *(unsigned __int8 *)(80 * a5 + dword_53A45 + 1);
  result = dword_53AA9 - 1;
  if ( v5 > dword_53AA9 - 1 )
  {
    result = dword_51A87 + dword_53AA9;
    if ( v5 < dword_51A87 + dword_53AA9 )
    {
      result = dword_53AAD - 1;
      if ( v6 >= dword_53AAD - 1 )
      {
        result = dword_51A8B + dword_53AAD;
        if ( v6 <= dword_51A8B + dword_53AAD )
        {
          for ( i = 0; i < 4; ++i )
          {
            v9 = 5 * i;
            if ( i == 1 )
              v10 = v9 + 3;
            else
              v10 = v9 + 2;
            byte_53D34[i + dword_53EC4] = v10;
            result = dword_53EC4;
            byte_53DFC[i + dword_53EC4] = a5;
            byte_53C6C[i + result] = *((_BYTE *)v11 + i);
          }
          dword_53EC4 += 4;
        }
      }
    }
  }
  return result;
}
