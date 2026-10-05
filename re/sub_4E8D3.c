char __cdecl sub_4E8D3(__int16 *a1, int a2, int a3, int a4, int a5, int a6)
{
  _BYTE *v6; // esi
  _BYTE *v7; // edi
  int v8; // edx
  unsigned int v9; // ecx
  int v10; // eax
  __int16 v11; // bx
  char v12; // cl
  _BOOL1 v13; // cf
  _BYTE *v14; // edi

  word_627B4 = *a1;
  v6 = a1 + 2;
  word_627B6 = a1[1];
  v7 = (_BYTE *)(a2 + a5 * a3 + a4);
  v8 = a5 - (unsigned __int16)word_627B4;
  v9 = 0;
  v10 = 0;
  do
  {
    v11 = word_627B4;
    do
    {
      while ( 1 )
      {
        while ( 1 )
        {
          LOBYTE(v10) = *v6++;
          v12 = 2 * v10;
          if ( (v10 & 0x80u) != 0 )
            break;
          v13 = __CFSHL__(v12, 1);
          LOBYTE(v9) = 4 * v10;
          if ( v13 )
          {
            LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
            v11 = v11 - v9 - v9;
            LOBYTE(v10) = *v6++;
            LOBYTE(v10) = *(_BYTE *)(v10 + a6);
            do
            {
              v14 = v7 + 1;
              *v14 = v10;
              v7 = v14 + 1;
              --v9;
            }
            while ( v9 != 0 );
            if ( v11 == 0 )
              goto LABEL_17;
          }
          else
          {
            LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
            v11 -= v9;
            LOBYTE(v10) = *v6++;
            LOBYTE(v10) = *(_BYTE *)(v10 + a6);
            memset(v7, v10, v9);
            v7 += v9;
            v9 = 0;
            if ( v11 == 0 )
              goto LABEL_17;
          }
        }
        v13 = __CFSHL__(v12, 1);
        LOBYTE(v9) = 4 * v10;
        if ( !v13 )
          break;
        LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
        v7 += v9;
        v11 -= v9;
        if ( v11 == 0 )
          goto LABEL_17;
      }
      LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
      v11 -= v9;
      do
      {
        LOBYTE(v10) = *v6++;
        LOBYTE(v10) = *(_BYTE *)(v10 + a6);
        *v7++ = v10;
        --v9;
      }
      while ( v9 != 0 );
      v9 = 0;
    }
    while ( v11 != 0 );
LABEL_17:
    v7 += v8;
    --word_627B6;
  }
  while ( word_627B6 != 0 );
  return v10;
}
