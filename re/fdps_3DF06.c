void __cdecl sub_3DF06(int a1)
{
  int v1; // edx
  unsigned int i; // ebx
  unsigned int j; // ebx

  v1 = ++dword_69E6C;
  if ( dword_69E80 != 0 && (v1 == 1 || dword_69E84 != 0) && sub_44CB2() == 0 && sub_3D3E6() != 0 )
    fprintf(dword_69E7C, "AIL_register_timer(0x%X)\n", a1);
  sub_44CBE(a1);
  if ( dword_69E80 != 0 && (dword_69E6C == 1 || dword_69E84 != 0) && sub_44CB2() == 0 )
  {
    for ( i = 0; i < 0xE; ++i )
      fprintf(dword_69E7C, " ");
    for ( j = 1; j < dword_69E6C; ++j )
      fprintf(dword_69E7C, byte_62279);
    JUMPOUT(0x3E8AC);
  }
  JUMPOUT(0x3E8B4);
}
