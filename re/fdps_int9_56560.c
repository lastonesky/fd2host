int __fastcall sub_56560(int a1, int a2, int a3)
{
  __asm { int     21h; DOS - 2+ - GET INTERRUPT VECTOR }
  dword_70002 = a3;
  word_70000 = __ES__;
  __asm { int     21h; DOS - SET INTERRUPT VECTOR }
  return 9481;
}
