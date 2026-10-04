int __cdecl sub_30270(int a1)
{
  int v1; // eax
  int i; // [esp+0h] [ebp-4h]

  v1 = sub_3D488();
  byte_69D72 = 0;
  byte_69D71 = 0;
  byte_69D70 = 1;
  dword_69D6C = sub_401BC(v1);
  if ( dword_69D6C != 0 )
  {
    byte_69D72 = 1;
    dword_69D5C = sub_403ED(dword_69D6C);
  }
  dword_69D68 = sub_3E7D5();
  if ( dword_69D68 != 0 )
  {
    byte_69D71 = 1;
    for ( i = 0; i < 8; ++i )
      dword_69D30[i] = sub_3EA1A(dword_69D68);
  }
  return sub_30540(a1);
}
