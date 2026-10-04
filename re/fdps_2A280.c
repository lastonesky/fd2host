int sub_2A280()
{
  int v0; // eax
  int v1; // eax
  int v2; // eax
  _DWORD v4[5]; // [esp+0h] [ebp-88h] BYREF
  int v5; // [esp+14h] [ebp-74h]
  int v6; // [esp+18h] [ebp-70h]
  int v7; // [esp+1Ch] [ebp-6Ch]
  int v8; // [esp+20h] [ebp-68h]
  _DWORD v9[3]; // [esp+24h] [ebp-64h] BYREF
  _BYTE v10[12]; // [esp+30h] [ebp-58h] BYREF
  int v11; // [esp+3Ch] [ebp-4Ch]
  int i; // [esp+40h] [ebp-48h]
  int v13; // [esp+44h] [ebp-44h]
  int v14; // [esp+48h] [ebp-40h]
  int v15; // [esp+4Ch] [ebp-3Ch]
  int v16; // [esp+50h] [ebp-38h]
  int v17; // [esp+54h] [ebp-34h]
  int v18; // [esp+58h] [ebp-30h]
  int v19; // [esp+5Ch] [ebp-2Ch]
  int v20; // [esp+60h] [ebp-28h]
  int v21; // [esp+64h] [ebp-24h]
  int v22; // [esp+68h] [ebp-20h]
  void *v23; // [esp+6Ch] [ebp-1Ch]
  int v24; // [esp+70h] [ebp-18h]
  int v25; // [esp+74h] [ebp-14h]
  int v26; // [esp+78h] [ebp-10h]
  int v27; // [esp+80h] [ebp-8h]
  char v28; // [esp+84h] [ebp-4h]

  v11 = 0;
  v14 = 0;
  v23 = &loc_20200;
  v26 = 0;
  v27 = 0;
  sub_56523(&loc_20200);
  byte_60159 = 1;
  dword_64110 = 0;
  v0 = fopen(aFdeSav, &aBasrb[3]);
  v25 = v0;
  if ( v0 != 0 )
  {
    v19 = malloc(22987);
    fread(v19, 1, 22987, v25);
    fclose(v25);
    sub_56627(v19, 22987);
    v20 = v19 + 12483;
    if ( sub_56608(v19, 22987) == *(_DWORD *)(v19 + 22983) && *(unsigned __int8 *)(v20 + 2) != 255 )
      *(_WORD *)((char *)&v23 + 1) = 0;
    v0 = free(v19);
  }
  while ( v26 == 0 )
  {
    sub_3C217(v0);
    v14 = 0;
    v17 = sub_2A110(aMiscVfs, aDynastyPal);
    v4[0] = malloc(&loc_16480);
    v4[1] = 368;
    v4[2] = 248;
    v4[3] = 24;
    v4[4] = 24;
    v5 = sub_2A110(aMiscVfs, aLogoSaf);
    v6 = 0;
    v7 = 0;
    v8 = 0;
    v9[2] = v5;
    sub_14540(v9, 1);
    sub_22F10(v17, 0, 255, 0, 0, 0);
    free(v17);
    while ( v14 == 0 )
    {
      v6 = v9[0];
      sub_14140(v4, 1);
      v14 = sub_14540(v9, 0);
      if ( (unsigned __int8)sub_5652E() < 0x80u )
        v14 = 1;
      while ( (inp(986) & 8) == 0 )
        ;
      while ( (inp(986) & 8) != 0 )
        ;
      sub_2F080(v4[0] + 8856, 368, 655360, 320, 320, 200);
      while ( v13 == dword_69D64 )
        ;
      v13 = dword_69D64;
    }
    free(v4[0]);
    free(v5);
    v14 = 0;
    sprintf(v10, "FD%d", v27 + 1);
    sub_30CB0(v10);
    byte_60008 = 1;
    sub_30960(1);
    while ( v14 == 0 )
    {
      v18 = sub_2A110(aMiscVfs, &aFiestselCel[3]);
      v21 = sub_2A110(aMiscVfs, &aNdstboardCel[2]);
      v17 = sub_2A110(aMiscVfs, aStboardPal);
      sub_22F10(v17, 0, 255, 0, 0, 0);
      v22 = malloc(64000);
      v28 = 0;
      v16 = 1500;
      while ( v28 == 0 )
      {
        v24 = (unsigned __int8)sub_5652E();
        if ( (v24 == 28 || v24 == 57) && *((_BYTE *)&v23 + v11) == 0 )
        {
          v28 = 1;
        }
        else if ( v24 == 72 )
        {
          v11 = (v11 + 3) % 4;
        }
        else if ( v24 == 80 )
        {
          v11 = (v11 + 1) % 4;
        }
        memset(v22, 0, 64000);
        sub_2D930(v21, 0, v22, 320, 0, 0, 0, 0);
        for ( i = 0; i < 4; ++i )
        {
          if ( v11 == i )
            v15 = 1;
          else
            v15 = *((unsigned __int8 *)&v23 + i);
          sub_2D930(v18, v15 + 3 * i, v22, 320, 119, 17 * i + 72, 0, 0);
        }
        while ( (inp(986) & 8) == 0 )
          ;
        while ( (inp(986) & 8) != 0 )
          ;
        memmove(655360, v22, 64000);
        while ( v13 == dword_69D64 )
          ;
        v13 = dword_69D64;
        if ( --v16 == 0 )
          v28 = 1;
      }
      for ( i = 0; i < 10; ++i )
      {
        while ( (inp(986) & 8) == 0 )
          ;
        sub_22F10(v17, 0, 255, -6 * i, -6 * i, -6 * i);
        while ( v13 == dword_69D64 )
          ;
        v13 = dword_69D64;
      }
      memset(655360, 0, 64000);
      free(v18);
      free(v21);
      free(v22);
      free(v17);
      v1 = sub_22F10(dword_643CC, 0, 255, 0, 0, 0);
      if ( v16 != 0 )
      {
        if ( v11 != 0 )
        {
          if ( v11 == 1 )
          {
            v17 = malloc(64000);
            memset(v17, 0, 64000);
            v14 = sub_24460(v17);
            if ( v14 == -1 )
              v14 = 0;
            v2 = free(v17);
            byte_60131 = 1;
            if ( v14 == 1 )
              sub_30F80(v2);
          }
          else if ( v11 == 2 )
          {
            sub_23DF0(v1);
            v14 = 1;
          }
          else
          {
            v14 = 1;
            byte_643EB = 1;
          }
        }
        else
        {
          dword_64114 = 0;
          dword_69CF4 = 0;
          byte_60158 = 0;
          funcs_2A82E();
          byte_60158 = 1;
          v14 = 1;
        }
        dword_69CD0 = 1;
      }
      if ( v16 == 0 )
        v14 = 1;
    }
    if ( v16 != 0 )
      v26 = 1;
    v0 = (v27 + 1) / 2;
    v27 = (v27 + 1) % 2;
  }
  return v11;
}
