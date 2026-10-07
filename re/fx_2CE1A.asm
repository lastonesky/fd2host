0x2CE1A  push    18h
0x2CE1F  call    sub_3702F
0x2CE24  movzx   eax, [esp+arg_10]
0x2CE29  test    eax, eax
0x2CE2B  jnz     short loc_2CE41
0x2CE2D  mov     byte_5412F, 0
0x2CE34  mov     byte_5412E, 1
0x2CE3B  mov     eax, 14h
0x2CE40  retn
0x2CE41  cmp     eax, 3
0x2CE44  jnz     short loc_2CE4C
0x2CE46  mov     eax, 3Ch ; '<'
0x2CE4B  retn
0x2CE4C  cmp     eax, 6
0x2CE4F  jz      short loc_2CE3B
0x2CE51  cmp     eax, 1
0x2CE54  jz      short loc_2CE5B
0x2CE56  cmp     eax, 7
0x2CE59  jnz     short loc_2CE8C
0x2CE5B  cmp     byte_5412F, 0
0x2CE62  jnz     short loc_2CE82
0x2CE64  push    0FFFFFFFFh
0x2CE66  push    [esp+4+arg_C]
0x2CE6A  push    [esp+8+arg_8]
0x2CE6E  movzx   eax, byte_5412F
0x2CE75  push    eax
0x2CE76  push    [esp+10h+arg_4]
0x2CE7A  call    sub_2EB9F
0x2CE7F  add     esp, 14h
0x2CE82  xor     byte_5412F, 1
0x2CE89  xor     eax, eax
0x2CE8B  retn
0x2CE8C  cmp     eax, 4
0x2CE8F  jnz     short loc_2CEAC
0x2CE91  push    0FFFFFFFFh
0x2CE93  push    [esp+4+arg_C]
0x2CE97  push    [esp+8+arg_8]
0x2CE9B  push    0
0x2CE9D  push    [esp+10h+arg_4]
0x2CEA1  call    sub_2EB9F
0x2CEA6  add     esp, 14h
0x2CEA9  xor     eax, eax
0x2CEAB  retn
0x2CEAC  cmp     eax, 5
0x2CEAF  jnz     loc_2CF2D
0x2CEB5  push    0FFFFFFFFh
0x2CEB7  push    [esp+4+arg_C]
0x2CEBB  push    [esp+8+arg_8]
0x2CEBF  movzx   edx, byte_5412E
0x2CEC6  mov     eax, edx
0x2CEC8  sar     edx, 1Fh
0x2CECB  sub     eax, edx
0x2CECD  sar     eax, 1
0x2CECF  push    eax
0x2CED0  push    [esp+10h+arg_4]
0x2CED4  call    sub_2EB9F
0x2CED9  add     esp, 14h
0x2CEDC  movzx   eax, byte_5412E
0x2CEE3  cmp     eax, 6
0x2CEE6  jnz     short loc_2CEF9
0x2CEE8  push    1
0x2CEEA  push    1
0x2CEEC  push    dword_54153
0x2CEF2  call    sub_25A96
0x2CEF7  jmp     short loc_2CF0D
0x2CEF9  cmp     eax, 24h ; '$'
0x2CEFC  jnz     short loc_2CF10
0x2CEFE  push    1
0x2CF00  push    2
0x2CF02  push    dword_54153
0x2CF08  call    sub_25B45
0x2CF0D  add     esp, 0Ch
0x2CF10  inc     byte_5412E
0x2CF16  movzx   eax, byte_5412E
0x2CF1D  cmp     eax, 2Ch ; ','
0x2CF20  jge     short loc_2CF2D
0x2CF22  cmp     eax, 10h
0x2CF25  jle     short loc_2CF2D
0x2CF27  mov     eax, 1
0x2CF2C  retn
0x2CF2D  xor     eax, eax
0x2CF2F  retn
