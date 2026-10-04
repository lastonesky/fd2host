int __fastcall sub_10B4E(__int32 a1, int a2, int a3, int a4, int a5)
{
  int v5; // esi
  int i; // ebx
  int v7; // ebx

  sub_3702F(a1, a2, a3, a4, 32);
  v5 = fopen(aFdiconB24_0, aRb_11);
  if ( v5 == 0 )
  {
    word_53A8D = 3;
    int386(16, &word_53A8D, &word_53A8D);
    JUMPOUT(0x10056);
  }
  dword_53A59 = sub_111BA(aFdfieldDat, dword_53A59, 3 * dword_53C03 + 2);
  for ( i = 0; i < dword_53BE3; ++i )
  {
    if ( *(unsigned __int8 *)(dword_53A55 + 26 * i + 152) == a5 )
      sub_10C50(i, v5);
  }
  fclose(v5);
  free(dword_53A59);
  dword_53A59 = 0;
  v7 = fopen(aFd2Tmp_0, aWb_1);
  fwrite(dword_53A61, 1, (char *)&loc_329FE + 2, v7);
  return fclose(v7);
}
