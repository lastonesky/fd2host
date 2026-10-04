int __cdecl _dospawn(int a1, int a2, int a3, int a4)
{
  int v4; // ecx
  unsigned __int16 v5; // ax
  _DWORD v7[10]; // [esp-20h] [ebp-28h] BYREF
  int savedregs; // [esp+8h] [ebp+0h] BYREF

  v7[6] = v4;
  v7[4] = (unsigned __int16)__ES__;
  v7[3] = (unsigned __int16)__DS__;
  dword_61528 = a4;
  word_6152C = __DS__;
  dword_61534 = 0;
  dword_6153A = 0;
  word_61538 = 0;
  word_6153E = 0;
  dword_61540 = 0;
  dword_61544 = 0;
  dword_6152E = a3;
  word_61532 = __DS__;
  v7[2] = &savedregs;
  v7[1] = (unsigned __int16)__DS__;
  v7[0] = (unsigned __int16)__DS__;
  word_6154C = __SS__;
  dword_61548 = (int)v7;
  word_6154E = __DS__;
  if ( a1 == 1 )
    LOBYTE(v5) = 4;
  else
    LOBYTE(v5) = 0;
  __asm { int     21h; DOS - CHECK STANDARD INPUT STATUS }
  __asm { int     21h; DOS - 2+ - LOAD OR EXECUTE (EXEC) }
  dword_60448 = 0;
  HIBYTE(v5) = 77;
  __asm { int     21h; DOS - 2+ - GET EXIT CODE OF SUBPROGRAM (WAIT) }
  return dosretax(v5, 0);
}
