0x2CAFC  push    50h ; 'P'
0x2CB01  call    sub_3702F
0x2CB06  push    ebx
0x2CB07  push    esi
0x2CB08  push    edi
0x2CB09  push    ebp
0x2CB0A  sub     esp, 28h
0x2CB0D  xor     ebp, ebp
0x2CB0F  mov     ecx, 0Ah
0x2CB14  mov     edi, esp
0x2CB16  mov     esi, offset unk_5261E
0x2CB1B  rep movsd
0x2CB1D  mov     edx, [esp+38h+arg_0]
0x2CB21  mov     eax, edx
0x2CB23  shl     eax, 2
0x2CB26  add     eax, edx
0x2CB28  shl     eax, 4
0x2CB2B  mov     edx, dword_53A45
0x2CB31  movzx   eax, byte ptr [edx+eax+6]
0x2CB36  test    eax, eax
0x2CB38  jnz     short loc_2CB4B
0x2CB3A  xor     ebx, ebx
0x2CB3C  jmp     short loc_2CB46
0x2CB3E  add     [esp+ebx*4+38h+var_38], 82h
0x2CB45  inc     ebx
0x2CB46  cmp     ebx, 0Ah
0x2CB49  jl      short loc_2CB3E
0x2CB4B  movzx   eax, [esp+38h+arg_10]
0x2CB50  test    eax, eax
0x2CB52  jnz     short loc_2CB95
0x2CB54  xor     ebx, ebx
0x2CB56  jmp     short loc_2CB72
0x2CB58  mov     edx, ebx
0x2CB5A  neg     edx
0x2CB5C  mov     eax, edx
0x2CB5E  shl     eax, 2
0x2CB61  sub     eax, edx
0x2CB63  mov     dword_540CB[ebx*4], eax
0x2CB6A  mov     dword_540DB[ebx*4], ebx
0x2CB71  inc     ebx
0x2CB72  cmp     ebx, 4
0x2CB75  jl      short loc_2CB58
0x2CB77  mov     byte_540EB, bl
0x2CB7D  mov     byte_540EC, 0
0x2CB84  mov     byte_540ED, 0
0x2CB8B  mov     eax, 2
0x2CB90  jmp     loc_2CCEC
0x2CB95  cmp     eax, 3
0x2CB98  jnz     short loc_2CBA4
0x2CB9A  mov     eax, 20h ; ' '
0x2CB9F  jmp     loc_2CCEC
0x2CBA4  cmp     eax, 6
0x2CBA7  jnz     short loc_2CBBA
0x2CBA9  mov     byte_540EC, 1
0x2CBB0  mov     eax, 10h
0x2CBB5  jmp     loc_2CCEC
0x2CBBA  cmp     eax, 2
0x2CBBD  jz      short loc_2CBCD
0x2CBBF  cmp     eax, 5
0x2CBC2  jz      short loc_2CBCD
0x2CBC4  cmp     eax, 8
0x2CBC7  jnz     loc_2CCEA
0x2CBCD  inc     byte_540ED
0x2CBD3  movzx   edx, byte_540ED
0x2CBDA  mov     ebx, 2
0x2CBDF  mov     eax, edx
0x2CBE1  sar     edx, 1Fh
0x2CBE4  idiv    ebx
0x2CBE6  mov     byte_540ED, dl
0x2CBEC  xor     ebx, ebx
0x2CBEE  mov     esi, 0Ah
0x2CBF3  jmp     loc_2CC6A
0x2CBF8  cmp     ebx, 1
0x2CBFB  jnz     short loc_2CC0D
0x2CBFD  push    ebx
0x2CBFE  push    ebx
0x2CBFF  push    dword_54153
0x2CC05  call    sub_25B45
0x2CC0A  add     esp, 0Ch
0x2CC0D  inc     dword_540CB[ebx*4]
0x2CC14  cmp     dword_540CB[ebx*4], 2
0x2CC1C  jnz     short loc_2CC23
0x2CC1E  mov     ebp, 1
0x2CC23  mov     ecx, ebx
0x2CC25  shl     ecx, 2
0x2CC28  cmp     dword_540CB[ecx], 7
0x2CC2F  jnz     short loc_2CC69
0x2CC31  movzx   eax, byte_540EC
0x2CC38  test    eax, eax
0x2CC3A  jnz     short loc_2CC69
0x2CC3C  inc     byte_540EB
0x2CC42  movzx   edx, byte_540EB
0x2CC49  mov     eax, edx
0x2CC4B  sar     edx, 1Fh
0x2CC4E  idiv    esi
0x2CC50  mov     byte_540EB, dl
0x2CC56  movzx   eax, dl
0x2CC59  mov     dword_540DB[ecx], eax
0x2CC5F  mov     dword_540CB[ecx], 0
0x2CC69  inc     ebx
0x2CC6A  cmp     ebx, 3
0x2CC6D  jge     loc_2CCE6
0x2CC73  mov     eax, ebx
0x2CC75  shl     eax, 2
0x2CC78  cmp     dword_540CB[eax], 0
0x2CC7F  jl      short loc_2CCB1
0x2CC81  cmp     dword_540CB[eax], 5
0x2CC88  jge     short loc_2CCB1
0x2CC8A  push    0FFFFFFFFh
0x2CC8C  push    [esp+3Ch+arg_C]
0x2CC90  mov     edx, dword_540DB[eax]
0x2CC96  mov     ecx, [esp+40h+arg_8]
0x2CC9A  add     ecx, [esp+edx*4+40h+var_38]
0x2CC9E  push    ecx
0x2CC9F  push    dword_540CB[eax]
0x2CCA5  push    [esp+48h+arg_4]
0x2CCA9  call    sub_2EB9F
0x2CCAE  add     esp, 14h
0x2CCB1  movzx   eax, byte_540ED
0x2CCB8  test    eax, eax
0x2CCBA  jnz     short loc_2CC69
0x2CCBC  cmp     dword_540CB[ebx*4], 1
0x2CCC4  jnz     loc_2CC0D
0x2CCCA  test    ebx, ebx
0x2CCCC  jnz     loc_2CBF8
0x2CCD2  push    1
0x2CCD4  push    1
0x2CCD6  push    dword_54153
0x2CCDC  call    sub_25A96
0x2CCE1  jmp     loc_2CC0A
0x2CCE6  mov     eax, ebp
0x2CCE8  jmp     short loc_2CCEC
0x2CCEA  xor     eax, eax
0x2CCEC  add     esp, 28h
0x2CCEF  pop     ebp
0x2CCF0  pop     edi
0x2CCF1  pop     esi
0x2CCF2  pop     ebx
0x2CCF3  retn
