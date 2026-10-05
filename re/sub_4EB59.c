void __cdecl __spoils<> sub_4EB59(int a1, int a2, int a3)
{
  _BYTE *v3; // esi
  _BYTE *v4; // edi
  int v5; // ebp
  int v6; // ecx
  char v7; // ah
  char v8; // dh
  char v9; // dl
  _BYTE *v10; // [esp-24h] [ebp-24h]

  v3 = (_BYTE *)(a3 + 1284);
  v4 = (_BYTE *)(a2 + 1284);
  HIWORD(v5) = 0;
  HIWORD(v6) = 0;
  v7 = -64;
  v8 = a1;
  do
  {
    v10 = v3;
    LOWORD(v6) = 312;
    v9 = a1;
    do
    {
      *v4++ = *v3;
      if ( --v9 == 0 )
      {
        v9 = a1;
        v3 += a1;
      }
      --v6;
    }
    while ( v6 != 0 );
    v3 = v10;
    v4 += 8;
    if ( --v8 == 0 )
    {
      v8 = a1;
      LOWORD(v5) = 320 * a1;
      v3 = &v10[v5];
    }
    --v7;
  }
  while ( v7 != 0 );
}
