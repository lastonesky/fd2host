__int64 __usercall sub_4ECF0@<edx:eax>(int a1@<ebp>, char *a2@<edi>, char *a3@<esi>)
{
  __int64 result; // rax
  unsigned __int16 v4; // cx

  WORD2(result) = word_627B6;
  do
  {
    v4 = word_627B4;
    qmemcpy(a2, a3, (unsigned __int16)word_627B4);
    a2 += v4;
    a3 += a1 + v4;
    --WORD2(result);
  }
  while ( WORD2(result) != 0 );
  return result;
}
