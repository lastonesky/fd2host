int __cdecl sub_3B9E8(int a1)
{
  _BYTE v2[4]; // [esp+0h] [ebp-10h] BYREF
  _BYTE v3[4]; // [esp+4h] [ebp-Ch] BYREF
  _BYTE v4[8]; // [esp+8h] [ebp-8h] BYREF

  sub_3B9AF(a1, v2, v4, v3);
  return 75 * v4[0] + 4500 * v2[0] + v3[0] - 150;
}
