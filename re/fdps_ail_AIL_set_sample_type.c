int __cdecl sub_3EE60(int a1, int a2, int a3)
{
  int v3; // edx
  int result; // eax

  v3 = ++dword_69E6C;
  if ( dword_69E80 != 0 && (v3 == 1 || dword_69E84 != 0) && sub_44CB2() == 0 && sub_3D3E6() != 0 )
    fprintf(dword_69E7C, "AIL_set_sample_type(0x%X,%d,%u)\n", a1, a2, a3);
  result = sub_47040(a1, a2, a3);
  --dword_69E6C;
  return result;
}
