int __cdecl main(int argc, const char **argv, const char **envp)
{
  int v3; // eax
  int v4; // eax
  int v5; // eax
  int v6; // eax
  int v7; // eax
  int v8; // eax
  int v9; // eax
  int v10; // eax
  _WORD v12[14]; // [esp+4h] [ebp-38h] BYREF
  char v13; // [esp+20h] [ebp-1Ch] BYREF
  int v14; // [esp+34h] [ebp-8h]
  int v15; // [esp+38h] [ebp-4h]

  if ( access(aDiskNo, 0) != 0 )
  {
    printf(aCanTFoundFileD);
    printf(aPleaseUseInsta);
    exit(1);
  }
  v15 = fopen(aDiskNo_0, &unk_61C2C);
  fscanf(v15, "%s", &v13);
  fscanf(v15, "%s", &v13);
  fscanf(v15, "%s", &unk_643E8);
  v3 = fclose(v15);
  dword_69D54 = -1;
  v14 = (__int16)sub_3C3A6(v3);
  if ( v14 != 1 )
  {
    printf(aFatalErrorCdro);
    printf(aCheckYourCdrom);
    exit(1);
  }
  v4 = sub_30270(25);
  sub_29630(v4);
  v12[0] = 19;
  v5 = int386(16, v12, v12);
  dword_69CF0 = 0;
  dword_69CDC = 0;
  byte_643EB = 0;
  dword_69DA0 = 0;
  v6 = sub_2A280(v5);
  while ( byte_643EB == 0 )
  {
    sub_2B870(v6);
    v6 = dword_69DA0;
    if ( dword_69DA0 != 0 )
    {
      if ( (unsigned int)dword_69DA0 <= 1 )
      {
        v7 = sub_2A920();
        v6 = sub_2A280(v7);
      }
      else if ( dword_69DA0 == 2 )
      {
        v8 = funcs_29371[dword_69CF4]();
        v6 = sub_30F80(v8);
      }
    }
    dword_69DA0 = 0;
  }
  v9 = sub_29410(v6);
  v10 = sub_30330(v9);
  sub_3C217(v10);
  v12[0] = 3;
  int386(16, v12, v12);
  return printf(aThankYouForPla);
}
