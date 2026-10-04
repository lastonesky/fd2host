int sub_2B870()
{
  int result; // eax
  int v1; // eax
  int v2; // eax
  int v3; // eax
  _BYTE *v4; // [esp+0h] [ebp-50h]
  int v5; // [esp+4h] [ebp-4Ch]
  int v6; // [esp+8h] [ebp-48h]
  unsigned __int8 *v7; // [esp+Ch] [ebp-44h]
  int v8; // [esp+10h] [ebp-40h]
  int v9; // [esp+14h] [ebp-3Ch]
  _BYTE *v10; // [esp+18h] [ebp-38h]
  int v11; // [esp+20h] [ebp-30h]
  int v12; // [esp+24h] [ebp-2Ch]
  int v13; // [esp+28h] [ebp-28h]
  int v14; // [esp+2Ch] [ebp-24h]
  int v15; // [esp+30h] [ebp-20h]
  int v16; // [esp+34h] [ebp-1Ch]
  int v17; // [esp+38h] [ebp-18h]
  int v18; // [esp+3Ch] [ebp-14h]
  int v19; // [esp+40h] [ebp-10h]
  int v20; // [esp+44h] [ebp-Ch]
  int i; // [esp+48h] [ebp-8h]
  int v22; // [esp+48h] [ebp-8h]

  v12 = 0;
  v13 = 0;
  v20 = 0;
  v10 = (_BYTE *)sub_56509();
  *v10 = -1;
  v15 = 24 * *(__int16 *)dword_60144;
  result = 24 * *(__int16 *)(dword_60144 + 2);
  v16 = result;
  while ( v13 == 0 )
  {
    sub_309C0();
    v14 = (unsigned __int8)*v10;
    if ( v14 == v11 )
    {
      v1 = v12++;
    }
    else
    {
      v1 = (unsigned __int8)*v10;
      v11 = v1;
      v12 = 0;
    }
    if ( v12 != 0 && v12 <= 5 )
    {
      if ( v14 == 59 )
      {
        sub_2DEF0(v1);
      }
      else if ( v14 == 60 || v14 == 71 )
      {
        v3 = sub_56523(v1);
        v22 = sub_2D830(v3);
        if ( v22 != -1 )
          goto LABEL_52;
      }
    }
    else if ( v14 == 72 && dword_69CCC >= 24 )
    {
      dword_69CCC -= 24;
      sub_2A1C0(aBeepWav_1);
    }
    else if ( v14 == 80 && (v1 = v16 - 24, v16 - 24 > dword_69CCC) )
    {
      dword_69CCC += 24;
      sub_2A1C0(aBeepWav_1);
    }
    else if ( v14 == 75 && dword_69CD4 >= 24 )
    {
      dword_69CD4 -= 24;
      sub_2A1C0(aBeepWav_1);
    }
    else if ( v14 == 77 && (v1 = v15 - 24, v15 - 24 > dword_69CD4) )
    {
      dword_69CD4 += 24;
      sub_2A1C0(aBeepWav_1);
    }
    else if ( v14 == 1 || v14 == 44 || v14 == 76 || v14 == 83 )
    {
      v19 = v20;
      for ( i = 0; i < dword_60150; ++i )
      {
        if ( (*(_BYTE *)(80 * v19 + dword_69CD8 + 5) & 0x85) == 0 && *(_BYTE *)(80 * v19 + dword_69CD8 + 6) == 2 )
        {
          v9 = v19;
          v8 = v19;
          v7 = (unsigned __int8 *)(80 * v19 + dword_69CD8);
          sub_2D550(24 * *v7, 24 * v7[1]);
          v20 = (v19 + 1) % dword_60150;
          break;
        }
        v19 = (v19 + 1) % dword_60150;
      }
    }
    else if ( v14 == 57 || v14 == 28 )
    {
      v2 = sub_56523(v1);
      v22 = sub_2D830(v2);
      if ( v22 == -1 )
      {
        v13 = sub_14AA0();
        goto LABEL_53;
      }
      v6 = v22;
      v5 = v22;
      v4 = (_BYTE *)(80 * v22 + dword_69CD8);
      dword_69CEC = 0;
      if ( v4[6] != 2 || (v4[5] & 0x80) != 0 || v4[38] != 0 )
      {
LABEL_52:
        sub_16A90(v22);
        goto LABEL_53;
      }
      sub_15460(v22);
      if ( dword_69DA0 == 0 )
        sub_2E7A0(v4, v22, v22);
    }
LABEL_53:
    v17 = dword_69CD4 - dword_69CE4;
    v18 = dword_69CCC - dword_69CE0;
    if ( dword_69CE4 >= 24 && v17 < 24 )
      dword_69CE4 -= 24;
    if ( v17 > 264 && v15 - 312 > dword_69CE4 )
      dword_69CE4 += 24;
    if ( dword_69CE0 >= 24 && v18 < 24 )
      dword_69CE0 -= 24;
    if ( v18 > 144 && v16 - 192 > dword_69CE0 )
      dword_69CE0 += 24;
    result = sub_2BC40(v4, v5, v6, v7, v8, v9);
    if ( dword_69DA0 != 0 )
      v13 = 1;
  }
  return result;
}
