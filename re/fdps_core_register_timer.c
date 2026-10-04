unsigned int __cdecl sub_44CBE(int a1)
{
  unsigned int v1; // eax
  unsigned int v3; // [esp-4h] [ebp-10h]

  sub_3D7B4();
  v1 = 0;
  while ( *(int *)((char *)&dword_604A0 + v1) != 0 )
  {
    v1 += 4;
    if ( v1 >= 0x3C )
    {
      v1 = 0xFFFFFFFF;
      goto LABEL_6;
    }
  }
  *(int *)((char *)&dword_604A0 + v1) = 1;
  *(int *)((char *)&dword_60460 + v1) = a1;
LABEL_6:
  v3 = v1;
  sub_3D7B9();
  return v3;
}
