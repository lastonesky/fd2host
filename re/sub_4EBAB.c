void __cdecl __spoils<> sub_4EBAB(_BYTE *a1, __int16 *a2, int a3)
{
  __int16 v4; // bp
  __int16 v5; // dx
  int v6; // ecx
  __int16 v7; // ax
  int v8; // ecx
  _BYTE *v9; // [esp-24h] [ebp-24h]

  v4 = *a2;
  v5 = a2[1];
  HIWORD(v6) = 0;
  v7 = 0;
  do
  {
    v9 = a1;
    LOWORD(v6) = v4;
    do
    {
      v7 = sub_4EC66(v7, v5, a3, v6);
      if ( (_BYTE)v7 != 0 )
        *a1 = v7;
      ++a1;
      v6 = v8 - 1;
    }
    while ( v6 != 0 );
    a1 = &v9[a3];
    --v5;
  }
  while ( v5 != 0 );
}
