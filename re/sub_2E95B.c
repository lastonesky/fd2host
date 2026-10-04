int __fastcall sub_2E95B(__int32 a1, int a2, int a3, int a4)
{
  int v4; // ebx

  sub_3702F(a1, a2, a3, a4, 24);
  v4 = fopen(aFd2Tmp_1, &unk_502BA);
  dword_53A61 = malloc((char *)&loc_329FE + 2);
  sub_373CA((_BYTE *)dword_53A61, 1u, (int)&loc_329FE + 2, v4);
  return fclose(v4);
}
