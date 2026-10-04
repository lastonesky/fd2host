unsigned int __cdecl sub_3B9AF(int a1, _BYTE *a2, _BYTE *a3, _BYTE *a4)
{
  unsigned int result; // eax

  *a4 = a1;
  *a3 = (unsigned __int16)(a1 & 0xFF00) >> 8;
  result = (a1 & 0xFF0000u) >> 16;
  *a2 = result;
  return result;
}
