int __usercall sub_44516@<eax>(
        __int32 a1@<eax>,
        int a2@<edx>,
        int a3@<ecx>,
        int a4@<ebx>,
        int a5@<ebp>,
        __int32 a6@<edi>,
        __int32 a7@<esi>)
{
  int v7; // edx
  unsigned int i; // edi
  unsigned int v9; // eax
  unsigned int j; // edi
  __int32 v11; // eax
  __int32 v12; // ecx
  __int32 v13; // eax
  _DWORD v15[10]; // [esp-30h] [ebp-30h] BYREF
  __int32 v16; // [esp-8h] [ebp-8h] BYREF
  __int32 v17; // [esp-4h] [ebp-4h] BYREF
  _DWORD retaddr[2]; // [esp+0h] [ebp+0h] BYREF

  if ( dword_605F2 != 0 )
  {
    v17 = a1;
    __outbyte(0x20u, 0x20u);
    __asm { iret }
  }
  v17 = a7;
  v16 = a6;
  v15[9] = a5;
  v15[8] = retaddr;
  v15[7] = a4;
  v15[6] = a3;
  v15[5] = a2;
  v15[4] = a1;
  v15[3] = (unsigned __int16)__DS__;
  v15[2] = (unsigned __int16)__ES__;
  v15[1] = (unsigned __int16)__FS__;
  v15[0] = (unsigned __int16)__GS__;
  __DS__ = word_605FA;
  dword_605F2 = 1;
  word_61008 = __SS__;
  dword_6100C = (int)v15;
  v7 = dword_605EE;
  for ( i = 0; i < 16; ++i )
  {
    if ( dword_604A0[i] == 2 )
    {
      v9 = v7 + dword_604E0[i];
      if ( v9 >= dword_60520[i] )
      {
        v9 -= dword_60520[i];
        ++dword_60560[i];
      }
      dword_604E0[i] = v9;
    }
  }
  __outbyte(0x20u, 0x20u);
  _enable();
  if ( dword_605F6 <= 0 )
  {
    for ( j = 0; j < 15; ++j )
    {
      while ( dword_60560[j] != 0 )
      {
        --dword_60560[j];
        ((void (__stdcall *)(int))dword_60460[j])(dword_605A0[j]);
      }
    }
  }
  if ( dword_6059C == 0 )
  {
    --dword_605F2;
    __asm { iret }
  }
  --dword_6059C;
  --dword_605F2;
  v11 = (unsigned __int16)word_605E4;
  v12 = _InterlockedExchange(&v16, dword_605E0);
  v13 = _InterlockedExchange(&v17, v11);
  return MK_FP(retaddr[0], retaddr[0])(v13, v17, v16, v12);
}
