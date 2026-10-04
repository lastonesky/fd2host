int __fastcall sub_30CB0(int a1, int a2, int a3)
{
  int v3; // eax
  int i; // eax
  int v5; // eax
  _BYTE v7[20]; // [esp+0h] [ebp-3Ch] BYREF
  _BYTE v8[20]; // [esp+14h] [ebp-28h] BYREF
  _BYTE v9[20]; // [esp+28h] [ebp-14h] BYREF

  v3 = sub_3D622();
  for ( i = sub_56588(v3); sub_432E0(i) != 0; i = getch() )
    ;
  sub_3C217();
  sub_22F10(dword_643CC, 0, 255, -64, -64, -64);
  sprintf((int)v7, (int)aSFdExe, (char)&unk_643E8);
  sprintf((int)v8, (int)aSSVid, (char)&unk_643E8);
  sprintf((int)v9, (int)aSSAud, (char)&unk_643E8);
  spawnlp(0, (int)v7, v7, v8, v9, 0);
  memset(a3, 655360, 0, 64000);
  v5 = sub_22F10(dword_643CC, 0, 255, 0, 0, 0);
  sub_56560(v5);
  return sub_30270(25);
}
