void __cdecl sub_3E3F2(int a1)
{
  int v1; // edx

  v1 = ++dword_69E6C;
  if ( dword_69E80 != 0 && (v1 == 1 || dword_69E84 != 0) && sub_44CB2() == 0 && sub_3D3E6() != 0 )
    fprintf(dword_69E7C, "AIL_stop_timer(%u)\n", a1);
  sub_44DCC(a1);
  JUMPOUT(0x3DA14);
}
