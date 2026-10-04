int sub_4469F()
{
  unsigned int v0; // eax
  unsigned int v1; // ecx
  unsigned int i; // edi

  v0 = sub_3D7B4((unsigned __int16)__ES__);
  v1 = 0xFFFFFFFF;
  for ( i = 0; i < 16; ++i )
  {
    if ( dword_604A0[i] != 0 )
    {
      v0 = dword_60520[i];
      if ( v0 < v1 )
        v1 = dword_60520[i];
    }
  }
  if ( v1 != dword_605EE )
  {
    dword_605EE = v1;
    sub_4466C(v1);
    v0 = 0;
    memset(dword_604E0, 0, 0x40u);
  }
  return sub_3D7B9(v0);
}
