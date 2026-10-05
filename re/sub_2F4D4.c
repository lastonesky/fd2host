int __fastcall sub_2F4D4(__int32 a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, __int16 *a9)
{
  int i; // esi
  int result; // eax
  int j; // esi

  sub_3702F(a1, a2, a3, a4, 40);
  for ( i = 9; i >= 0; --i )
  {
    sub_4E98D((__int16 *)dword_5413F[i % 3], 0, 50, a8, 640, -1);
    sub_11EB0(655360, 320, a8 + 32 * i, 640, 320, 200);
  }
  memset(a8, 0, &loc_1F400);
  memset(a7, 0, 64000);
  sub_4E98D(a9, 0, 50, a7, 320, -1);
  sub_2FACD(a7, a5);
  sub_11EB0(a8, 640, a7, 320, 320, 200);
  result = sub_2EB9F(a6, 0, a8, 640, -1);
  for ( j = 9; j >= 0; --j )
  {
    sub_4E98D((__int16 *)dword_5413F[(j + 2) % 3], 0, 50, a8 + 320, 640, -1);
    result = sub_11EB0(655360, 320, a8 + 32 * j, 640, 320, 200);
  }
  return result;
}
