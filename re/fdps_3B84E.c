int sub_3B84E()
{
  int result; // eax

  memset(&word_69DF0, 0, 12);
  word_69DC8 = 256;
  word_69DCC = 32;
  int386x(49, &word_69DC8, &dword_69DAC, &word_69DF0);
  word_69E54 = dword_69DAC;
  dword_69DE8 = 16 * (unsigned __int16)dword_69DAC;
  int386x(49, &word_69DC8, &dword_69DAC, &word_69DF0);
  dword_69DA8 = (unsigned __int16)dword_69DAC << 16;
  result = 16 * (unsigned __int16)dword_69DAC;
  dword_69DA4 = result;
  return result;
}
