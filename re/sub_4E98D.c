char __cdecl sub_4E98D(__int16 *a1, int a2, int a3, int a4, int a5, int a6)
{
  char *v6; // esi
  char *v7; // edi
  int v8; // edx
  unsigned int v9; // ecx
  __int16 v10; // bx
  char result; // al
  char v12; // cl
  _BOOL1 v13; // cf
  char *v14; // edi
  __int16 v15; // bx
  char v16; // cl
  _BYTE *v17; // edi
  __int16 v18; // bp
  char v19; // cl
  char v20; // al
  char v21; // al
  char *v22; // edi
  char v23; // al

  word_627B4 = *a1;
  v6 = (char *)(a1 + 2);
  word_627B6 = a1[1];
  v7 = (char *)(a2 + a5 * a3 + a4);
  v8 = a5 - (unsigned __int16)word_627B4;
  v9 = 0;
  if ( a6 == -1 )
  {
    do
    {
      v10 = word_627B4;
      do
      {
        while ( 1 )
        {
          while ( 1 )
          {
            result = *v6++;
            v12 = 2 * result;
            if ( result >= 0 )
              break;
            v13 = __CFSHL__(v12, 1);
            LOBYTE(v9) = 4 * result;
            if ( v13 )
            {
              LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
              v7 += v9;
              v10 -= v9;
              if ( v10 == 0 )
                goto LABEL_17;
            }
            else
            {
              LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
              v10 -= v9;
              qmemcpy(v7, v6, v9);
              v6 += v9;
              v7 += v9;
              v9 = 0;
              if ( v10 == 0 )
                goto LABEL_17;
            }
          }
          v13 = __CFSHL__(v12, 1);
          LOBYTE(v9) = 4 * result;
          if ( v13 )
            break;
          LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
          v10 -= v9;
          result = *v6++;
          memset(v7, result, v9);
          v7 += v9;
          v9 = 0;
          if ( v10 == 0 )
            goto LABEL_17;
        }
        LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
        v10 = v10 - v9 - v9;
        result = *v6++;
        do
        {
          v14 = v7 + 1;
          *v14 = result;
          v7 = v14 + 1;
          --v9;
        }
        while ( v9 != 0 );
      }
      while ( v10 != 0 );
LABEL_17:
      v7 += v8;
      --word_627B6;
    }
    while ( word_627B6 != 0 );
  }
  else if ( (unsigned __int16)a6 > 0xFFu )
  {
    do
    {
      v18 = word_627B4;
      do
      {
        while ( 1 )
        {
          while ( 1 )
          {
            result = *v6++;
            v19 = 2 * result;
            if ( result < 0 )
              break;
            v13 = __CFSHL__(v19, 1);
            LOBYTE(v9) = 4 * result;
            if ( v13 )
            {
              LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
              v18 = v18 - v9 - v9;
              v21 = *v6++;
              result = a6 + ((BYTE1(a6) + v21) & 7);
              do
              {
                v22 = v7 + 1;
                *v22 = result;
                v7 = v22 + 1;
                --v9;
              }
              while ( v9 != 0 );
              if ( v18 == 0 )
                goto LABEL_49;
            }
            else
            {
              LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
              v18 -= v9;
              v20 = *v6++;
              result = a6 + ((BYTE1(a6) + v20) & 7);
              memset(v7, result, v9);
              v7 += v9;
              v9 = 0;
              if ( v18 == 0 )
                goto LABEL_49;
            }
          }
          v13 = __CFSHL__(v19, 1);
          LOBYTE(v9) = 4 * result;
          if ( !v13 )
            break;
          LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
          v7 += v9;
          v18 -= v9;
          if ( v18 == 0 )
            goto LABEL_49;
        }
        LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
        v18 -= v9;
        do
        {
          v23 = *v6++;
          result = a6 + ((BYTE1(a6) + v23) & 7);
          *v7++ = result;
          --v9;
        }
        while ( v9 != 0 );
      }
      while ( v18 != 0 );
LABEL_49:
      v7 += v8;
      --word_627B6;
    }
    while ( word_627B6 != 0 );
  }
  else
  {
    do
    {
      v15 = word_627B4;
      do
      {
        while ( 1 )
        {
          while ( 1 )
          {
            result = *v6++;
            v16 = 2 * result;
            if ( result >= 0 )
              break;
            v13 = __CFSHL__(v16, 1);
            LOBYTE(v9) = 4 * result;
            if ( v13 )
            {
              LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
              v7 += v9;
              v15 -= v9;
              if ( v15 == 0 )
                goto LABEL_32;
            }
            else
            {
              LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
              v15 -= v9;
              v6 += v9;
              result = a6;
              memset(v7, a6, v9);
              v7 += v9;
              v9 = 0;
              if ( v15 == 0 )
                goto LABEL_32;
            }
          }
          v13 = __CFSHL__(v16, 1);
          LOBYTE(v9) = 4 * result;
          if ( v13 )
            break;
          LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
          v15 -= v9;
          ++v6;
          result = a6;
          memset(v7, a6, v9);
          v7 += v9;
          v9 = 0;
          if ( v15 == 0 )
            goto LABEL_32;
        }
        LOBYTE(v9) = ((unsigned __int8)v9 >> 2) + 1;
        v15 = v15 - v9 - v9;
        ++v6;
        result = a6;
        do
        {
          v17 = v7 + 1;
          *v17 = a6;
          v7 = v17 + 1;
          --v9;
        }
        while ( v9 != 0 );
      }
      while ( v15 != 0 );
LABEL_32:
      v7 += v8;
      --word_627B6;
    }
    while ( word_627B6 != 0 );
  }
  return result;
}
