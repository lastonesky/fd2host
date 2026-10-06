15FC4  cmp     eax, 0FFFFFFFDh
15FC7  jnz     loc_16049
15FCD  cmp     dword_53C67, 728h
15FD7  jz      short loc_15FE5
15FD9  cmp     dword_53C67, 9017h
15FE3  jnz     short loc_15FF5
15FE5  cmp     [esp+34h+var_18], 3
15FEA  jnz     short loc_15FF5
15FEC  call    sub_16E24
15FF1  dec     [esp+34h+var_18]
15FF5  mov     eax, [esp+34h+arg_C]
15FF9  imul    eax, [esp+34h+arg_1C]
15FFE  inc     [esp+34h+var_18]
16002  imul    eax, [esp+34h+var_18]
16007  mov     ebp, [esp+34h+arg_8]
1600B  add     ebp, eax
1600D  add     esi, 2
16010  cmp     dword_53C67, 728h
1601A  jz      short loc_16028
1601C  cmp     dword_53C67, 9017h
16026  jnz     short loc_16032
16028  push    0
1602A  call    sub_16559
1602F  add     esp, 4
16032  push    1
16034  call    sub_16C57
16039  add     esp, 4
1603C  mov     [esp+34h+arg_20], 1
16044  jmp     loc_1630D
16049  lea     edx, [esi+2]
1604C  mov     [esp+34h+var_28], edx
16050  cmp     eax, 0FFFFFFFCh
16053  jnz     short loc_16086
16055  push    1
16057  push    13h
16059  push    4Ah ; 'J'
1605B  push    4Ch ; 'L'
1605D  push    0CDh
16062  push    [esp+48h+arg_C]
16066  push    ebp
16067  push    dword_53AD9
1606D  push    dword_53A7D
16073  call    sub_15F84
16078  add     esp, 24h
1607B  mov     ebp, eax
1607D  mov     esi, [esp+34h+var_28]
16081  jmp     loc_1630D
16086  cmp     eax, 0FFFFFFFBh
16089  jnz     short loc_160A5
1608B  push    1
1608D  push    13h
1608F  push    4Ah ; 'J'
16091  push    4Ch ; 'L'
16093  push    0CDh
16098  push    [esp+48h+arg_C]
1609C  push    ebp
1609D  push    dword_53ADD
160A3  jmp     short loc_1606D
160A5  cmp     eax, 0FFFFFFFAh
160A8  jnz     loc_16140
160AE  push    dword_53AE1
160B4  push    offset aD_0; "%d"
160B9  lea     eax, [esp+3Ch+var_34]
160BD  push    eax
160BE  call    sprintf
160C3  add     esp, 0Ch
160C6  mov     eax, esp
160C8  push    eax
160C9  call    strlen
160CE  add     esp, 4
160D1  mov     [esp+34h+var_14], al
160D5  mov     [esp+34h+var_24], 0
160DD  jmp     short loc_160E6
160DF  add     ebp, 10h
160E2  inc     [esp+34h+var_24]
160E6  movzx   eax, [esp+34h+var_14]
160EB  cmp     eax, [esp+34h+var_24]
160EF  jle     loc_16362
160F5  push    [esp+34h+arg_18]
160F9  push    [esp+38h+arg_14]
160FD  push    [esp+3Ch+arg_10]
16101  push    [esp+40h+arg_C]
16105  push    ebp
16106  mov     eax, [esp+48h+var_24]
1610A  movzx   eax, [esp+eax+48h+var_34]
1610F  sub     eax, 30h ; '0'
16112  push    eax
16113  push    dword_53A75
16119  call    sub_4ED7A
1611E  add     esp, 1Ch
16121  call    sub_10620
16126  test    eax, eax
16128  jz      short loc_16132
1612A  mov     [esp+34h+arg_20], 0
16132  cmp     [esp+34h+arg_20], 0
16137  jz      short loc_160DF
16139  call    sub_164E8
1613E  jmp     short loc_160DF
16140  cmp     eax, 0FFFFFFEFh
16143  jnz     loc_1622A
16149  cmp     [esp+34h+var_1C], 0
1614E  jz      short loc_16174
16150  push    0
16152  call    sub_16559
16157  add     esp, 4
1615A  push    0
1615C  call    sub_16C57
16161  add     esp, 4
16164  push    [esp+34h+var_20]
16168  push    [esp+38h+var_1C]
1616C  call    sub_16B43
16171  add     esp, 8
16174  mov     dword_53C67, 728h
1617E  movzx   ebp, word ptr [esi+2]
16182  push    ebp
16183  call    sub_12C60
16188  add     esp, 4
1618B  cmp     eax, 0FFFFFFFFh
1618E  jz      short loc_1619A
16190  mov     [esp+34h+var_20], 2
16198  jmp     short loc_161A2
1619A  mov     [esp+34h+var_20], 0
161A2  cmp     ebp, 27h ; '''
161A5  jz      short loc_161B1
161A7  mov     edi, dword_53C1B
161AD  movzx   ebp, byte ptr [edi+7]
161B1  push    ebp
161B2  push    dword_53A85
161B8  push    offset aDatoDat; "DATO.DAT"
161BD  call    sub_111BA
161C2  add     esp, 0Ch
161C5  mov     dword_53A85, eax
161CA  push    [esp+34h+var_20]
161CE  movzx   eax, byte ptr [edi+1]
161D2  push    eax
161D3  movzx   eax, byte ptr [edi]
161D6  push    eax
161D7  call    sub_165AC
161DC  add     esp, 0Ch
161DF  mov     [esp+34h+var_1C], eax
161E3  mov     edi, dword_53A85
161E9  movzx   eax, byte ptr [edi]
161EC  add     edi, eax
161EE  push    140h
161F3  push    edi
161F4  mov     eax, 0A0000h
161F9  add     eax, dword_53C67
161FF  push    eax
16200  call    sub_4EBFF
16205  add     esp, 0Ch
16208  mov     [esp+34h+arg_20], 1
16210  mov     [esp+34h+var_18], 0
16218  mov     [esp+34h+arg_8], 0A0B4Fh
16220  mov     ebp, 0A0B4Fh
16225  jmp     loc_1630A
1622A  cmp     eax, 0FFFFFFEEh
1622D  jnz     loc_16367
16233  cmp     [esp+34h+var_1C], 0
16238  jz      short loc_1625E
1623A  push    0
1623C  call    sub_16559
16241  add     esp, 4
16244  push    0
16246  call    sub_16C57
1624B  add     esp, 4
1624E  push    [esp+34h+var_20]
16252  push    [esp+38h+var_1C]
16256  call    sub_16B43
1625B  add     esp, 8
1625E  mov     dword_53C67, 9017h
16268  movzx   ebp, word ptr [esi+2]
1626C  push    ebp
1626D  call    sub_12C60
16272  add     esp, 4
16275  cmp     eax, 0FFFFFFFFh
16278  jz      short loc_16284
1627A  mov     [esp+34h+var_20], 70h ; 'p'
16282  jmp     short loc_1628C
16284  mov     [esp+34h+var_20], 0
1628C  mov     edi, dword_53C1B
16292  movzx   ebp, byte ptr [edi+7]
16296  push    ebp
16297  push    dword_53A85
1629D  push    offset aDatoDat; "DATO.DAT"
162A2  call    sub_111BA
162A7  add     esp, 0Ch
162AA  mov     dword_53A85, eax
162AF  push    [esp+34h+var_20]
162B3  movzx   eax, byte ptr [edi+1]
162B7  push    eax
162B8  movzx   eax, byte ptr [edi]
162BB  push    eax
162BC  call    sub_165AC
162C1  add     esp, 0Ch
162C4  mov     [esp+34h+var_1C], eax
162C8  mov     edi, dword_53A85
162CE  movzx   eax, byte ptr [edi]
162D1  add     edi, eax
162D3  push    140h
162D8  push    edi
162D9  mov     eax, 0A0000h
162DE  add     eax, dword_53C67
162E4  push    eax
162E5  call    sub_4EC31
162EA  add     esp, 0Ch
162ED  mov     [esp+34h+arg_20], 1