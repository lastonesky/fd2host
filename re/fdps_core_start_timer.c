int __cdecl sub_44D78(int a1)
{
  int v1; // eax

  v1 = sub_3D7B4();
  if ( a1 != -1 && *(int *)((char *)&dword_604A0 + a1) == 1 )
    *(int *)((char *)&dword_604A0 + a1) = 2;
  return sub_3D7B9(v1);
}
