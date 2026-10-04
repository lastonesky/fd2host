void sub_565A7()
{
  char v0; // al
  char v1; // bl
  unsigned __int8 v2; // al

  _enable();
  v0 = __inbyte(0x60u);
  v1 = v0;
  v2 = __inbyte(0x61u);
  v2 |= 0x80u;
  __outbyte(0x61u, v2);
  __outbyte(0x61u, v2 & 0x7F);
  byte_70006 = v1;
  if ( v1 != byte_70021 )
  {
    byte_70021 = v1;
    if ( (unsigned __int8)v1 < 0x80u )
    {
      *MK_FP(3, &byte_7000F[dword_7001D++]) = v1;
      if ( dword_7001D == 10 )
        dword_7001D = 0;
    }
  }
  __outbyte(0x20u, 0x20u);
  __asm { iret }
}
