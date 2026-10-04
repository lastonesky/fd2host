void __usercall sub_10010(__int32 a1@<eax>, int a2@<edx>, int a3@<ecx>, int a4@<ebx>, unsigned __int8 *a5@<edi>)
{
  int v5; // ecx
  int v6; // ebp
  int v7; // eax
  int v8; // ebx
  __int64 v9; // rax
  int v10; // eax
  int v11; // ecx
  __int64 v12; // rax
  int v13; // ecx
  int v14; // ebx
  int i; // ebx
  __int64 v16; // rax
  int v17; // ebx
  int v18; // eax
  int v19; // eax
  int j; // ebx
  int v21; // esi
  int k; // ebx
  int v23; // edi
  int v24; // [esp+0h] [ebp-14h]

  sub_3702F(a1, a2, a4, a3, 60);
  v6 = malloc(22987);
  if ( v6 != 0 )
  {
    v9 = fopen(aFd2Sav_2, aRb_0);
    v8 = v9;
    sub_373CA((_BYTE *)v6, 1u, 22987, v9);
    fclose(v8);
    sub_4DF28(v6, 22987);
    LODWORD(v9) = sub_4DF09(v6, 22987);
    if ( (_DWORD)v9 != *(_DWORD *)(v6 + 22983) )
    {
      LODWORD(v9) = sub_1956B(75);
      sub_15F84(v9, SHIDWORD(v9), v5, v8, a5, dword_53A7D, 436, 696099, 320, 205, 76, 74, 19, 1);
      sub_16559(0);
      v10 = sub_16C57(0);
      LODWORD(v9) = sub_196CB(v10);
    }
    sub_1F882(v9);
    memmove(dword_53BF7, v6 + 2211, 2560);
    dword_53A65 = sub_111BA(aFdotherDat, dword_53A65, 0);
    dword_53C03 = *(unsigned __int8 *)(v6 + 12485);
    v12 = sub_111BA(aFdfieldDat, dword_53A59, 3 * dword_53C03 + 2);
    dword_53A59 = v12;
    if ( dword_53A55 != 0 )
      free(dword_53A55);
    dword_53A55 = malloc(2211);
    if ( dword_53A55 != 0 )
    {
      LODWORD(v12) = memmove(dword_53A55, v6, 2211);
      sub_10652(v12, SHIDWORD(v12), v8, v11);
      dword_53A79 = sub_111BA(aFdtxtDat, dword_53A79, dword_53C03 + 1);
      dword_53A51 = sub_111BA(aFdfieldDat, dword_53A51, 3 * dword_53C03);
      dword_53AC1 = *(__int16 *)dword_53A51;
      dword_53AC5 = *(__int16 *)(dword_53A51 + 2);
      v14 = 2 * *(unsigned __int8 *)dword_53A55;
      dword_53A5D = sub_111BA(aFdshapDat, dword_53A5D, v14);
      dword_53A69 = sub_111BA(aFdshapDat, dword_53A69, v14 + 1);
      sub_4DF4C((unsigned __int8 *)dword_53A51);
      dword_53BE7 = *(unsigned __int8 *)(dword_53A55 + 1);
      dword_53BE3 = *(unsigned __int8 *)(dword_53A55 + 2);
      dword_53BEB = *(unsigned __int8 *)(v6 + 12484);
      if ( dword_53A45 != 0 )
        free(dword_53A45);
      dword_53A45 = malloc(7680);
      if ( dword_53A45 != 0 )
      {
        memmove(dword_53A45, v6 + 4771, 80 * dword_53BEB);
        memmove(dword_53AD5, v6 + 12451, 32);
        if ( dword_53A61 != 0 )
          free(dword_53A61);
        v24 = fopen(aFdiconB24, aRb_1);
        dword_53BDF = 0;
        for ( i = 0; i < dword_53BEB; ++i )
          *(_BYTE *)(80 * i + dword_53A45 + 2) = sub_11019(*(unsigned __int8 *)(80 * i + dword_53A45 + 7), v24);
        fclose(v24);
        v16 = fopen(aFd2Tmp, aWb_0);
        v17 = v16;
        fwrite(dword_53A61, 1, (char *)&loc_329FE + 2, v16);
        fclose(v17);
        dword_53BEF = *(unsigned __int8 *)(v6 + 12483);
        dword_53AA9 = *(unsigned __int8 *)(v6 + 12486);
        dword_53AAD = *(unsigned __int8 *)(v6 + 12487);
        LODWORD(qword_53AB1) = *(unsigned __int8 *)(v6 + 12488);
        HIDWORD(qword_53AB1) = *(unsigned __int8 *)(v6 + 12489);
        dword_53AB9 = *(unsigned __int8 *)(v6 + 12490);
        dword_53ABD = *(unsigned __int8 *)(v6 + 12491);
        dword_53BFB = *(unsigned __int8 *)(v6 + 12492);
        dword_53BF3 = *(_DWORD *)(v6 + 12493);
        byte_53AF9 = *(_BYTE *)(v6 + 12497);
        byte_51AAB = *(_BYTE *)(v6 + 12498);
        byte_51E61 = *(_BYTE *)(v6 + 12499);
        byte_51E62 = *(_BYTE *)(v6 + 12500);
        free(v6);
        free(dword_53A59);
        dword_53A59 = 0;
        v18 = sub_25977(
                (unsigned __int8)byte_51E63[dword_53C03],
                SHIDWORD(v16),
                v17,
                v13,
                (unsigned __int8)byte_51E63[dword_53C03],
                0);
        dword_51A83 = 0;
        sub_12263(v18);
        v19 = sub_11CAC(1);
        sub_1F525(v19);
        for ( j = 0; j < 9; ++j )
        {
          v21 = sub_15F0E(dword_53A81, 655360, 320, 120, 84, j + 83);
          if ( j > 6 )
            sub_187D6(684651, 320, dword_53BEF, 42, 3);
          j___delay(70);
          if ( j == 8 )
            j___delay(500);
          sub_15E71(v21, 655360, 320);
        }
        for ( k = 2; k < 6; ++k )
        {
          if ( k == 5 )
            k = 9;
          v23 = sub_15F0E(dword_53A81, dword_53A49 + 32904, 456, 116, k * k + 84, 91);
          sub_187D6(456 * (k * k + 90) + dword_53A49 + 33071, 456, dword_53BEF, 42, 3);
          sub_11EB0(656644, 320, dword_53A49 + 32904, 456, 312, 192);
          sub_17AA9(1);
          sub_15E71(v23, dword_53A49 + 32904, 456);
        }
        sub_11CAC(0);
        j___delay(200);
        dword_53AE9 = 0;
        dword_51A83 = 1;
        sub_4E381();
        JUMPOUT(0x22BBE);
      }
      word_53A8D = 3;
      int386(16, &word_53A8D, &word_53A8D);
      v7 = printf(aOutOfMemory_1);
    }
    else
    {
      word_53A8D = 3;
      int386(16, &word_53A8D, &word_53A8D);
      v7 = printf(aOutOfMemory_0);
    }
  }
  else
  {
    word_53A8D = 3;
    int386(16, &word_53A8D, &word_53A8D);
    v7 = printf(aOutOfMemory);
  }
  exit(v7);
}
