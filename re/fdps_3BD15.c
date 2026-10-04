int sub_3BD15()
{
  int v0; // eax
  _WORD v2[13]; // [esp+0h] [ebp-24h] BYREF
  _BYTE v3[7]; // [esp+1Ch] [ebp-8h] BYREF

  v2[0] = 26;
  LOBYTE(v2[1]) = 3;
  HIBYTE(v2[6]) = 0;
  v2[10] = 0;
  *(_DWORD *)&v2[11] = 0;
  *(_DWORD *)&v2[7] = dword_69DA8;
  v2[9] = 7;
  memset(v3, 0, sizeof(v3));
  v3[0] = 10;
  memcpy(dword_69DE8, v2, 26);
  memcpy(dword_69DA4, v3, 7);
  v0 = memcpy(v3, dword_69DA4, sizeof(v3));
  sub_3B8ED(v0);
  memcpy(v3, dword_69DA4, sizeof(v3));
  memcpy(v2, dword_69DE8, sizeof(v2));
  memcpy(&unk_69E16, &v3[1], 6);
  byte_69E06 = v3[1];
  byte_69E07 = v3[2];
  sub_3B9AF(*(_DWORD *)&v3[3], &unk_69E08, &unk_69E09, &unk_69E0A);
  dword_69E0B = sub_3B9E8(*(_DWORD *)&v3[3]);
  word_69E20 = *(_WORD *)((char *)&v2[1] + 1);
  return *(_DWORD *)((char *)&v2[1] + 1);
}
