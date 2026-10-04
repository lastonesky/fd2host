int spawnlp(int a1, int a2, ...)
{
  va_list va; // [esp+10h] [ebp+Ch] BYREF

  va_start(va, a2);
  return spawnvp(a1, a2, (char *)va);
}
