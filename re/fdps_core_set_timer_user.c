__int32 __cdecl sub_44D05(int a1, __int32 a2)
{
  __int32 v2; // eax
  __int32 v4; // [esp-4h] [ebp-10h]

  v2 = sub_3D7B4();
  if ( a1 != -1 )
    v2 = _InterlockedExchange((int *)((char *)&dword_605A0 + a1), a2);
  v4 = v2;
  sub_3D7B9();
  return v4;
}
