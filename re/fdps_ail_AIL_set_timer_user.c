void __cdecl sub_3DFF1(int a1, int a2)
{
  int v2; // edx
  unsigned int i; // ebx
  unsigned int j; // ebx

  v2 = ++dword_69E6C;
  if ( dword_69E80 != 0 && (v2 == 1 || dword_69E84 != 0) && sub_44CB2() == 0 && sub_3D3E6() != 0 )
    fprintf(dword_69E7C, "AIL_set_timer_user(%u,%u)\n", a1, a2);
  sub_44D05(a1, a2);
  if ( dword_69E80 != 0 && (dword_69E6C == 1 || dword_69E84 != 0) && sub_44CB2() == 0 )
  {
    for ( i = 0; i < 0xE; ++i )
      fprintf(dword_69E7C, " ");
    for ( j = 1; j < dword_69E6C; ++j )
      fprintf(dword_69E7C, byte_62279);
    JUMPOUT(0x3D7A8);
  }
  JUMPOUT(0x3E8B4);
}
