int __cdecl sub_303C0(int a1, int a2, int a3)
{
  int i; // [esp+0h] [ebp-8h]

  if ( byte_69D71 == 0 || byte_69D70 == 0 )
    return -1;
  for ( i = 0; i < 8 && sub_3F26E(dword_69D30[i]) == 4; ++i )
    ;
  if ( i == 8 )
    return -1;
  sub_3EC6B(dword_69D30[i]);
  sub_3EDDE(dword_69D30[i], a1, a2);
  sub_3F1F8(dword_69D30[i], a3);
  sub_3F096(dword_69D30[i], dword_69D58);
  sub_3EEE2(dword_69D30[i]);
  return i;
}
