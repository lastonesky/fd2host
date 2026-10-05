void __usercall sub_15F84(
        __int32 a1@<eax>,
        int a2@<edx>,
        int a3@<ecx>,
        int a4@<ebx>,
        unsigned __int8 *a5@<edi>,
        int a6,
        int a7,
        int a8,
        int a9,
        int a10,
        int a11,
        int a12,
        int a13,
        int a14)
{
  int v14; // ebp
  __int16 *v15; // esi
  int v16; // eax
  int v17; // eax
  int v18; // ebp
  int v19; // eax
  unsigned __int8 *v20; // edi
  int v21; // eax
  int v22; // eax
  unsigned __int8 *v23; // edi
  int v24; // ebp
  unsigned __int8 *v25; // edi
  int v26; // ebp
  int v27; // eax
  _BYTE v28[12]; // [esp+0h] [ebp-34h] BYREF
  __int16 *v29; // [esp+Ch] [ebp-28h]
  int i; // [esp+10h] [ebp-24h]
  int v31; // [esp+14h] [ebp-20h]
  int v32; // [esp+18h] [ebp-1Ch]
  int v33; // [esp+1Ch] [ebp-18h]
  unsigned __int8 v34; // [esp+20h] [ebp-14h]
  unsigned __int8 *v35; // [esp+28h] [ebp-Ch]
  int v36; // [esp+30h] [ebp-4h]

  sub_3702F(a1, a2, a4, a3, 92);
  v36 = a4;
  v35 = a5;
  v33 = 0;
  v32 = 0;
  v31 = 0;
  v14 = a8;
  v15 = (__int16 *)(*(__int16 *)(a6 + 2 * a7) + a6);
  while ( 1 )
  {
    while ( 1 )
    {
      while ( 1 )
      {
        while ( 1 )
        {
          while ( 1 )
          {
            v22 = *v15;
            switch ( v22 )
            {
              case -3:
                if ( (dword_53C67 == 1832 || dword_53C67 == 36887) && v33 == 3 )
                {
                  sub_16E24();
                  --v33;
                }
                v14 = ++v33 * a13 * a9 + a8;
                ++v15;
                if ( dword_53C67 == 1832 || dword_53C67 == 36887 )
                  sub_16559(0);
                sub_16C57(1);
                a14 = 1;
                break;
              case -2:
                if ( (dword_53C67 == 1832 || dword_53C67 == 36887) && v33 == 3 )
                {
                  sub_16E24();
                  --v33;
                }
                v14 = ++v33 * a13 * a9 + a8;
                goto LABEL_50;
              case -1:
                if ( v32 != 0 )
                {
                  sub_16559(0);
                  sub_16C57(0);
                  sub_16B43(v32, v31);
                  dword_53C67 = 0;
                }
                JUMPOUT(0x15309);
              default:
                goto LABEL_11;
            }
          }
LABEL_11:
          v29 = v15 + 1;
          if ( v22 == -4 )
          {
            v16 = sub_15F84(dword_53A7D, dword_53AD9, v14, a9, 205, 76, 74, 19, 1);
            goto LABEL_13;
          }
          if ( v22 != -5 )
            break;
          v16 = sub_15F84(dword_53A7D, dword_53ADD, v14, a9, 205, 76, 74, 19, 1);
LABEL_13:
          v14 = v16;
          v15 = v29;
        }
        if ( v22 != -6 )
          break;
        sprintf(v28, "%d", dword_53AE1);
        v34 = strlen(v28);
        for ( i = 0; v34 > i; ++i )
        {
          v17 = sub_4ED7A(dword_53A75, (unsigned __int8)v28[i] - 48, v14, a9, a10, a11, a12);
          if ( sub_10620(v17) != 0 )
            a14 = 0;
          if ( a14 != 0 )
            sub_164E8();
          v14 += 16;
        }
LABEL_50:
        ++v15;
      }
      if ( v22 == -17 )
      {
        if ( v32 != 0 )
        {
          sub_16559(0);
          sub_16C57(0);
          sub_16B43(v32, v31);
        }
        dword_53C67 = 1832;
        v18 = (unsigned __int16)v15[1];
        if ( sub_12C60(v18) == -1 )
          v31 = 0;
        else
          v31 = 2;
        if ( v18 != 39 )
        {
          a5 = (unsigned __int8 *)dword_53C1B;
          v18 = *(unsigned __int8 *)(dword_53C1B + 7);
        }
        dword_53A85 = sub_111BA(aDatoDat, dword_53A85, v18);
        v19 = sub_165AC(*a5, a5[1], v31);
LABEL_33:
        v32 = v19;
        a5 = (unsigned __int8 *)(*(unsigned __int8 *)dword_53A85 + dword_53A85);
        sub_4EBFF(dword_53C67 + 655360, a5, 320);
        a14 = 1;
        v33 = 0;
        a8 = 658255;
        v14 = 658255;
        goto LABEL_42;
      }
      if ( v22 != -18 )
        break;
      if ( v32 != 0 )
      {
        sub_16559(0);
        sub_16C57(0);
        sub_16B43(v32, v31);
      }
      dword_53C67 = 36887;
      if ( sub_12C60((unsigned __int16)v15[1]) == -1 )
        v31 = 0;
      else
        v31 = 112;
      v20 = (unsigned __int8 *)dword_53C1B;
      dword_53A85 = sub_111BA(aDatoDat, dword_53A85, *(unsigned __int8 *)(dword_53C1B + 7));
      v21 = sub_165AC(*v20, v20[1], v31);
LABEL_41:
      v32 = v21;
      a5 = (unsigned __int8 *)(*(unsigned __int8 *)dword_53A85 + dword_53A85);
      sub_4EC31(dword_53C67 + 655360, a5, 320);
      a14 = 1;
      v33 = 0;
      a8 = 693535;
      v14 = 693535;
LABEL_42:
      v15 += 2;
    }
    if ( v22 == -19 )
    {
      if ( v32 != 0 )
      {
        sub_16559(0);
        sub_16C57(0);
        sub_16B43(v32, v31);
      }
      dword_53C67 = 1832;
      v23 = (unsigned __int8 *)(80 * (unsigned __int16)v15[1] + dword_53A45);
      v24 = v23[7];
      v31 = 2;
      dword_53A85 = sub_111BA(aDatoDat, dword_53A85, v24);
      v19 = sub_165AC(*v23, v23[1], 2);
      goto LABEL_33;
    }
    if ( v22 == -20 )
    {
      if ( v32 != 0 )
      {
        sub_16559(0);
        sub_16C57(0);
        sub_16B43(v32, v31);
      }
      dword_53C67 = 36887;
      v25 = (unsigned __int8 *)(80 * (unsigned __int16)v15[1] + dword_53A45);
      v26 = v25[7];
      v31 = 112;
      dword_53A85 = sub_111BA(aDatoDat, dword_53A85, v26);
      v21 = sub_165AC(*v25, v25[1], 112);
      goto LABEL_41;
    }
    v27 = sub_4ED7A(dword_53A75, v22, v14, a9, a10, a11, a12);
    v14 += 16;
    v15 = v29;
    if ( sub_10620(v27) != 0 )
      a14 = 0;
    if ( a14 != 0 )
      sub_164E8();
  }
}
