int __cdecl sub_44E20(int a1, int a2)
{
  int v2; // eax

  sub_3D7B4();
  *(int *)((char *)&dword_60520 + a1) = a2;
  *(int *)((char *)&dword_604E0 + a1) = 0;
  v2 = sub_4469F();
  return sub_3D7B9(v2);
}
