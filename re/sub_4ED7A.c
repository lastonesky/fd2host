void __cdecl __spoils<> sub_4ED7A(int a1, int a2, int a3, unsigned __int16 a4, char a5, char a6, int a7)
{
  int v7; // eax
  __int16 v8; // bx
  int v9; // eax
  char *v10; // edi
  char i; // bl
  __int16 *v12; // esi
  char *v13; // edi
  char v14; // dl
  char v15; // dh
  char j; // ch
  __int16 v17; // ax
  __int16 v18; // ax
  char v19; // t0
  char k; // cl
  _BOOL1 v21; // cf
  char *v22; // [esp-24h] [ebp-24h]

  dword_627AC = a1;
  dword_627B0 = a2;
  dword_627A8 = a3;
  word_627A3 = a4;
  byte_627A5 = a5;
  byte_627A7 = a6;
  v7 = a7;
  byte_627A6 = a7;
  if ( (_BYTE)a7 != 0 )
  {
    BYTE1(v7) = a7;
    v8 = v7;
    v9 = v7 << 16;
    LOWORD(v9) = v8;
    v10 = (char *)dword_627A8;
    for ( i = 16; i != 0; --i )
    {
      memset32(v10, v9, 4u);
      v10 += a4;
    }
  }
  if ( dword_627B0 != 10 )
  {
    v12 = (__int16 *)(32 * dword_627B0 + dword_627AC);
    v13 = (char *)dword_627A8;
    v14 = byte_627A5;
    v15 = byte_627A7;
    for ( j = 16; j != 0; --j )
    {
      v17 = *v12++;
      v19 = v17;
      LOBYTE(v18) = HIBYTE(v17);
      HIBYTE(v18) = v19;
      v22 = v13;
      for ( k = 16; k != 0; --k )
      {
        v21 = __CFSHL__(v18, 1);
        v18 *= 2;
        if ( v21 )
        {
          *v13 = v14;
          v13[a4 - 1] = v15;
          v13[a4] = v15;
        }
        ++v13;
      }
      v13 = &v22[a4];
    }
  }
}
