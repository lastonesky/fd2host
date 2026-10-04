// bad sp value at call has been detected, the output may be wrong!
void __usercall sub_19DF7(
        __int32 a1@<eax>,
        int a2@<edx>,
        int a3@<ecx>,
        int a4@<ebx>,
        int a5@<edi>,
        int a6@<esi>,
        int a7,
        int a8,
        int a9,
        ...)
{
  unsigned __int8 *v9; // edi
  int v10; // ebx
  _BYTE *v11; // esi
  int i; // edx
  int v13; // eax
  int v14; // ebx
  int v15; // eax
  __int32 v16; // eax
  int v17; // eax
  __int64 v18; // rax
  int v19; // ebx
  __int64 v20; // rax
  int v21; // ecx
  int v22; // eax
  int v23; // edi
  int v24; // eax
  __int32 v25; // eax
  int v26; // eax
  __int64 v27; // rax
  int v28; // eax
  __int32 v29; // eax
  int v30; // edx
  int v31; // ecx
  int v32; // eax
  int v33; // edx
  int v34; // ecx
  __int32 v35; // eax
  int v36; // eax
  __int64 v37; // rax
  int v38; // ebx
  __int32 v39; // eax
  int v40; // edx
  int v41; // ecx
  __int32 v42; // eax
  int v43; // edx
  int v44; // ecx
  int v45; // eax
  __int128 v46; // [esp-4h] [ebp-2Ch] BYREF
  __int128 v47; // [esp+Ch] [ebp-1Ch] BYREF
  int v48; // [esp+1Ch] [ebp-Ch]
  int v49; // [esp+20h] [ebp-8h]
  int v50; // [esp+24h] [ebp-4h]
  _UNKNOWN *retaddr; // [esp+28h] [ebp+0h] BYREF
  va_list va; // [esp+38h] [ebp+10h] BYREF

  va_start(va, a9);
  sub_3702F(a1, a2, a4, a3, 84);
  v50 = a4;
  v49 = a6;
  v48 = a5;
  v47 = unk_51EF5;
  v46 = unk_53F22;
  v9 = (unsigned __int8 *)&v47;
  v10 = fopen(aFd2Sav, aRb_14);
  if ( v10 != 0 )
  {
    v11 = (_BYTE *)malloc(22987);
    sub_373CA(v11, 1u, 22987, v10);
    fclose(v10);
    free(v11);
  }
  else
  {
    DWORD2(v46) = 1;
  }
  for ( i = 0; i < dword_53BEB; ++i )
  {
    v13 = 80 * i + dword_53A45;
    if ( (*(_BYTE *)(v13 + 5) & 1) == 0 && *(char *)(v13 + 5) < 0 )
      DWORD1(v46) = 1;
  }
  ((void (__cdecl *)(__int128 *, __int128 *))sub_1741C)(&v47, &v46);
  do
    v14 = sub_177FC((char *)va, &retaddr);
  while ( v14 == 0 );
  sub_176B4((char *)va, &retaddr);
  v15 = sub_11CAC(0);
  if ( v14 != -1 )
  {
    switch ( dword_53C57 )
    {
      case 0:
        sub_1B1E7(v15);
        goto LABEL_33;
      case 1:
        v16 = sub_1956B(75);
        sub_15F84(v16, i, 0, v14, (unsigned __int8 *)&v47, dword_53A7D, 410, 696099, 320, 205, 76, 74, 19, 1);
        v17 = sub_16559(0);
        v18 = sub_19953(v17);
        v19 = v18;
        LODWORD(v20) = sub_197E5(v18, HIDWORD(v18), v18);
        if ( v19 == 1 && dword_53C57 == 0 )
        {
          v19 = malloc(22987);
          v22 = fopen(aFd2Sav_0, aRb);
          v23 = v22;
          if ( v22 != 0 )
          {
            sub_373CA((_BYTE *)v19, 1u, 22987, v22);
            sub_4DF28(v19, 22987);
            fclose(v23);
          }
          else
          {
            *(_BYTE *)(v19 + 15147) = -1;
            *(_BYTE *)(v19 + 17747) = -1;
            *(_BYTE *)(v19 + 20347) = -1;
            *(_BYTE *)(v19 + 22947) = -1;
          }
          memmove(v19, dword_53A55, 2211);
          memmove(v19 + 2211, dword_53BF7, 2560);
          memmove(v19 + 4771, dword_53A45, 80 * dword_53BEB);
          memmove(v19 + 12451, dword_53AD5, 32);
          *(_BYTE *)(v19 + 12483) = dword_53BEF;
          *(_BYTE *)(v19 + 12484) = dword_53BEB;
          *(_BYTE *)(v19 + 12485) = dword_53C03;
          *(_BYTE *)(v19 + 12486) = dword_53AA9;
          *(_BYTE *)(v19 + 12487) = dword_53AAD;
          *(_BYTE *)(v19 + 12488) = qword_53AB1;
          *(_BYTE *)(v19 + 12489) = BYTE4(qword_53AB1);
          *(_BYTE *)(v19 + 12490) = dword_53AB9;
          *(_BYTE *)(v19 + 12491) = dword_53ABD;
          *(_BYTE *)(v19 + 12492) = dword_53BFB;
          *(_DWORD *)(v19 + 12493) = dword_53BF3;
          *(_BYTE *)(v19 + 12497) = byte_53AF9;
          *(_BYTE *)(v19 + 12498) = byte_51AAB;
          *(_BYTE *)(v19 + 12499) = byte_51E61;
          *(_BYTE *)(v19 + 12500) = byte_51E62;
          v20 = fopen(aFd2Sav_1, aWb);
          v9 = (unsigned __int8 *)v20;
          *(_DWORD *)(v19 + 22983) = sub_4DF09(v19, 22987);
          sub_4DF28(v19, 22987);
          fwrite(v19, 1, 22987, v9);
          fclose(v9);
          LODWORD(v20) = free(v19);
          v50 = 1;
          v49 = 19;
          v48 = 74;
          *((_QWORD *)&v47 + 1) = 0x4C000000CDLL;
          *(_QWORD *)&v47 = 0x140000AB6E3LL;
          HIDWORD(v46) = 411;
          goto LABEL_23;
        }
LABEL_22:
        v50 = 1;
        v49 = 19;
        v48 = 74;
        *((_QWORD *)&v47 + 1) = 0x4C000000CDLL;
        *(_QWORD *)&v47 = 0x140000AB6E3LL;
        HIDWORD(v46) = 412;
LABEL_23:
        sub_15F84(
          v20,
          SHIDWORD(v20),
          v21,
          v19,
          v9,
          dword_53A7D,
          SHIDWORD(v46),
          v47,
          SDWORD1(v47),
          SDWORD2(v47),
          SHIDWORD(v47),
          v48,
          v49,
          v50);
        v24 = j___delay(200);
        sub_196CB(v24);
LABEL_24:
        sub_4E381();
LABEL_33:
        JUMPOUT(0x16FD8);
      case 2:
        v25 = sub_1956B(75);
        sub_15F84(v25, i, 0, v14, (unsigned __int8 *)&v47, dword_53A7D, 413, 696099, 320, 205, 76, 74, 19, 1);
        v26 = sub_16559(0);
        v27 = sub_19953(v26);
        v19 = v27;
        LODWORD(v20) = sub_197E5(v27, HIDWORD(v27), v27);
        if ( v19 == 1 && dword_53C57 == 0 )
        {
          sub_15F84(
            v20,
            SHIDWORD(v20),
            v21,
            1,
            (unsigned __int8 *)&v47,
            dword_53A7D,
            414,
            702179,
            320,
            205,
            76,
            74,
            19,
            1);
          v28 = j___delay(200);
          v29 = sub_196CB(v28);
          v32 = sub_25977(v29, v30, 1, v31, -1, 0);
          sub_10010(v32, v33, v34, 1, (unsigned __int8 *)&v47);
          goto LABEL_24;
        }
        goto LABEL_22;
      default:
        break;
    }
    v35 = sub_1956B(75);
    sub_15F84(v35, i, 0, v14, (unsigned __int8 *)&v47, dword_53A7D, 415, 696099, 320, 205, 76, 74, 19, 1);
    v36 = sub_16559(0);
    v37 = sub_19953(v36);
    v38 = v37;
    v39 = sub_197E5(v37, HIDWORD(v37), v37);
    if ( v38 != 1 || dword_53C57 != 0 )
      JUMPOUT(0x1716F);
    sub_15F84(v39, v40, v41, 1, (unsigned __int8 *)&v47, dword_53A7D, 416, 702179, 320, 205, 76, 74, 19, 1);
    sub_25977(v42, v43, 1, v44, -1, 1);
    v45 = j___delay(200);
    sub_196CB(v45);
  }
  JUMPOUT(0x16FDD);
}
