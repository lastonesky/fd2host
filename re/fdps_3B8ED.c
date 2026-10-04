int sub_3B8ED()
{
  int result; // eax

  memset(&word_69DF0, 0, 12);
  memset(&unk_69E22, 0, 50);
  dword_69E3E = 5392;
  dword_69E3A = (unsigned __int8)byte_69DFE;
  dword_69E32 = 0;
  word_69E44 = word_69E54;
  word_69DC8 = 768;
  word_69DCC = 47;
  word_69DD0 = 0;
  word_69DF0 = __DS__;
  dword_69DDC = (int)&unk_69E22;
  result = int386x(49, &word_69DC8, &dword_69DAC, &word_69DF0);
  if ( dword_69DC4 != 0 )
    return printf(aDeviceRequestF);
  return result;
}
