int __cdecl sub_30350(int a1)
{
  int result; // eax
  int i; // [esp+0h] [ebp-4h]

  if ( a1 != -1 )
    return sub_3EF4F(dword_69D30[a1]);
  for ( i = 0; i < 8; ++i )
  {
    sub_3EF4F(dword_69D30[i]);
    result = i;
  }
  return result;
}
