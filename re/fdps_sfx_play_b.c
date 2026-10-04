int __cdecl sub_30790(int a1, int a2, int a3, int a4)
{
  char v5; // [esp+0h] [ebp-1Ch] BYREF
  char v6; // [esp+1h] [ebp-1Bh]
  int v7; // [esp+2h] [ebp-1Ah]
  int v8; // [esp+6h] [ebp-16h]
  int v9; // [esp+Ah] [ebp-12h]
  int i; // [esp+10h] [ebp-Ch]
  int v11; // [esp+14h] [ebp-8h]

  if ( byte_69D71 == 0 || byte_69D70 == 0 )
    return -1;
  for ( i = 0; i < 8 && sub_3F26E(dword_69D30[i]) == 4; ++i )
    ;
  if ( i == 8 )
    return -1;
  sub_305C0(a1, &v5);
  if ( v5 == 1 && v6 == 8 )
  {
    v11 = 0;
  }
  else if ( v5 == 1 && v6 == 16 )
  {
    v11 = 1;
  }
  else if ( v5 == 2 && v6 == 8 )
  {
    v11 = 2;
  }
  else
  {
    v11 = 3;
  }
  if ( a3 == -1 )
    a3 = v7;
  sub_3EC6B(dword_69D30[i]);
  sub_3EDDE(dword_69D30[i], v9, v8);
  sub_3EE60(dword_69D30[i], v11, 2);
  sub_3F1F8(dword_69D30[i], a2);
  sub_3F096(dword_69D30[i], a3);
  sub_3EEE2(dword_69D30[i]);
  if ( a4 != -1 )
    sub_3F10C(dword_69D30[i], a4);
  return i;
}
