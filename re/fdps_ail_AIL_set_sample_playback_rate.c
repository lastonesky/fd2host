void __cdecl sub_3F096(int a1, int a2)
{
  int v2; // edx

  v2 = ++dword_69E6C;
  if ( dword_69E80 != 0 && (v2 == 1 || dword_69E84 != 0) && sub_44CB2() == 0 && sub_3D3E6() != 0 )
    fprintf(dword_69E7C, "AIL_set_sample_playback_rate(0x%X,%d)\n", a1, a2);
  sub_47080(a1, a2);
  JUMPOUT(0x3D91A);
}
