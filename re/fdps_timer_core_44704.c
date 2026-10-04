void __fastcall sub_44704(int a1, unsigned __int16 a2)
{
  unsigned int v2; // kr00_4

  v2 = __readeflags();
  _disable();
  word_605FA = __DS__;
  sub_44ECE();
  dword_605F2 = 0;
  dword_605F6 = 0;
  dword_605EE = -1;
  dword_605FC = -1;
  memset(dword_604A0, 0, 0x40u);
  memset(dword_604E0, 0, 0x40u);
  memset(dword_60520, 0, 0x40u);
  memset(dword_60560, 0, 0x40u);
  __asm { int     31h; DPMI Services   ax=func xxxxh }
  __asm { int     21h; DOS - 2+ - GET INTERRUPT VECTOR }
  dword_605E0 = 8;
  word_605E4 = __DS__;
  dword_605E6 = a2;
  __asm { int     21h; DOS - SET INTERRUPT VECTOR }
  dword_604DC = 2;
  sub_3E0E4(60, 54925);
  _disable();
  if ( (v2 & 0x200) != 0 )
    _enable();
  __writeeflags(v2);
}
