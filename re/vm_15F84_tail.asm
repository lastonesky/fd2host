162F5  mov     [esp+34h+var_18], 0
162FD  mov     [esp+34h+arg_8], 0A951Fh
16305  mov     ebp, 0A951Fh
1630A  add     esi, 4
1630D  movsx   eax, word ptr [esi]
16310  cmp     eax, 0FFFFFFFFh
16313  jz      loc_164AC
16319  cmp     eax, 0FFFFFFFEh
1631C  jnz     loc_15FC4
16322  cmp     dword_53C67, 728h
1632C  jz      short loc_1633A
1632E  cmp     dword_53C67, 9017h
16338  jnz     short loc_1634A
1633A  cmp     [esp+34h+var_18], 3
1633F  jnz     short loc_1634A
16341  call    sub_16E24
16346  dec     [esp+34h+var_18]
1634A  mov     eax, [esp+34h+arg_C]
1634E  imul    eax, [esp+34h+arg_1C]
16353  inc     [esp+34h+var_18]
16357  imul    eax, [esp+34h+var_18]
1635C  mov     ebp, [esp+34h+arg_8]
16360  add     ebp, eax
16362  add     esi, 2
16365  jmp     short loc_1630D
16367  cmp     eax, 0FFFFFFEDh
1636A  jnz     short loc_163E3
1636C  cmp     [esp+34h+var_1C], 0
16371  jz      short loc_16397
16373  push    0
16375  call    sub_16559
1637A  add     esp, 4
1637D  push    0
1637F  call    sub_16C57
16384  add     esp, 4
16387  push    [esp+34h+var_20]
1638B  push    [esp+38h+var_1C]
1638F  call    sub_16B43
16394  add     esp, 8
16397  mov     dword_53C67, 728h
163A1  movzx   edi, word ptr [esi+2]
163A5  mov     eax, edi
163A7  shl     eax, 2
163AA  add     eax, edi
163AC  shl     eax, 4
163AF  mov     edi, dword_53A45
163B5  add     edi, eax
163B7  movzx   ebp, byte ptr [edi+7]
163BB  mov     [esp+34h+var_20], 2
163C3  push    ebp
163C4  push    dword_53A85
163CA  push    offset aDatoDat; "DATO.DAT"
163CF  call    sub_111BA
163D4  add     esp, 0Ch
163D7  mov     dword_53A85, eax
163DC  push    2
163DE  jmp     loc_161CE
163E3  cmp     eax, 0FFFFFFECh
163E6  jnz     short loc_1645F
163E8  cmp     [esp+34h+var_1C], 0
163ED  jz      short loc_16413
163EF  push    0
163F1  call    sub_16559
163F6  add     esp, 4
163F9  push    0
163FB  call    sub_16C57
16400  add     esp, 4
16403  push    [esp+34h+var_20]
16407  push    [esp+38h+var_1C]
1640B  call    sub_16B43
16410  add     esp, 8
16413  mov     dword_53C67, 9017h
1641D  movzx   edi, word ptr [esi+2]
16421  mov     eax, edi
16423  shl     eax, 2
16426  add     eax, edi
16428  shl     eax, 4
1642B  mov     edi, dword_53A45
16431  add     edi, eax
16433  movzx   ebp, byte ptr [edi+7]
16437  mov     [esp+34h+var_20], 70h ; 'p'
1643F  push    ebp
16440  push    dword_53A85
16446  push    offset aDatoDat; "DATO.DAT"
1644B  call    sub_111BA
16450  add     esp, 0Ch
16453  mov     dword_53A85, eax
16458  push    70h ; 'p'
1645A  jmp     loc_162B3
1645F  push    [esp+34h+arg_18]
16463  push    [esp+38h+arg_14]
16467  push    [esp+3Ch+arg_10]
1646B  push    [esp+40h+arg_C]
1646F  push    ebp
16470  push    eax
16471  push    dword_53A75
16477  call    sub_4ED7A
1647C  add     esp, 1Ch
1647F  add     ebp, 10h
16482  mov     esi, [esp+34h+var_28]
16486  call    sub_10620
1648B  test    eax, eax
1648D  jz      short loc_16497
1648F  mov     [esp+34h+arg_20], 0
16497  cmp     [esp+34h+arg_20], 0
1649C  jz      loc_1630D
164A2  call    sub_164E8
164A7  jmp     loc_1630D
164AC  cmp     [esp+34h+var_1C], 0
164B1  jz      short loc_164E1
164B3  push    0
164B5  call    sub_16559
164BA  add     esp, 4
164BD  push    0
164BF  call    sub_16C57
164C4  add     esp, 4
164C7  push    [esp+34h+var_20]
164CB  push    [esp+38h+var_1C]
164CF  call    sub_16B43
164D4  add     esp, 8
164D7  mov     dword_53C67, 0
164E1  mov     eax, ebp
164E3  jmp     loc_15309