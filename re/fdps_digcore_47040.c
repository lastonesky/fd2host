int __cdecl sub_47040(int a1, int a2, int a3)
{
  int result; // eax

  result = a1;
  if ( a1 != 0 && (a2 != *(_DWORD *)(a1 + 52) || a3 != *(_DWORD *)(a1 + 56)) )
  {
    *(_DWORD *)(a1 + 52) = a2;
    *(_DWORD *)(a1 + 56) = a3;
    return sub_45CC0(a1);
  }
  return result;
}
