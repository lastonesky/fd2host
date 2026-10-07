0x2CCF4  push    38h ; '8'
0x2CCF9  call    sub_3702F
0x2CCFE  push    ebx
0x2CCFF  push    esi
0x2CD00  push    edi
0x2CD01  push    ebp
0x2CD02  sub     esp, 10h
0x2CD05  mov     ebp, [esp+20h+arg_C]
0x2CD09  mov     ecx, 4
0x2CD0E  mov     edi, esp
0x2CD10  mov     esi, offset unk_52646
0x2CD15  rep movsd
0x2CD17  xor     esi, esi
0x2CD19  movzx   eax, [esp+20h+arg_10]
0x2CD1E  test    eax, eax
0x2CD20  jnz     short loc_2CD43
0x2CD22  xor     ebx, ebx
0x2CD24  jmp     short loc_2CD34
0x2CD26  mov     edx, ebx
0x2CD28  neg     edx
0x2CD2A  add     edx, edx
0x2CD2C  mov     dword_540EE[ebx*4], edx
0x2CD33  inc     ebx
0x2CD34  cmp     ebx, 10h
0x2CD37  jl      short loc_2CD26
0x2CD39  mov     eax, 3
0x2CD3E  jmp     loc_2CE12
0x2CD43  cmp     eax, 3
0x2CD46  jnz     short loc_2CD52
0x2CD48  mov     eax, 22h ; '"'
0x2CD4D  jmp     loc_2CE12
0x2CD52  cmp     eax, 6
0x2CD55  jnz     short loc_2CD61
0x2CD57  mov     eax, 2
0x2CD5C  jmp     loc_2CE12
0x2CD61  cmp     eax, 2
0x2CD64  jz      short loc_2CD6F
0x2CD66  cmp     eax, 5
0x2CD69  jnz     loc_2CE10
0x2CD6F  xor     ebx, ebx
0x2CD71  jmp     short loc_2CD74
0x2CD73  inc     ebx
0x2CD74  cmp     ebx, 10h
0x2CD77  jge     loc_2CE0C
0x2CD7D  mov     eax, ebx
0x2CD7F  shl     eax, 2
0x2CD82  cmp     dword_540EE[eax], 0
0x2CD89  jl      short loc_2CDB5
0x2CD8B  cmp     dword_540EE[eax], 8
0x2CD92  jge     short loc_2CDB5
0x2CD94  push    0FFFFFFFFh
0x2CD96  push    ebp
0x2CD97  push    [esp+28h+arg_8]
0x2CD9B  movzx   edx, [esp+ebx+2Ch+var_20]
0x2CDA0  mov     eax, dword_540EE[eax]
0x2CDA6  add     eax, edx
0x2CDA8  push    eax
0x2CDA9  push    [esp+30h+arg_4]
0x2CDAD  call    sub_2EB9F
0x2CDB2  add     esp, 14h
0x2CDB5  cmp     dword_540EE[ebx*4], 0
0x2CDBD  jnz     short loc_2CDD1
0x2CDBF  push    1
0x2CDC1  push    1
0x2CDC3  push    dword_54153
0x2CDC9  call    sub_25A96
0x2CDCE  add     esp, 0Ch
0x2CDD1  cmp     dword_540EE[ebx*4], 4
0x2CDD9  jnz     short loc_2CDED
0x2CDDB  push    1
0x2CDDD  push    2
0x2CDDF  push    dword_54153
0x2CDE5  call    sub_25B45
0x2CDEA  add     esp, 0Ch
0x2CDED  inc     dword_540EE[ebx*4]
0x2CDF4  cmp     dword_540EE[ebx*4], 4
0x2CDFC  jnz     loc_2CD73
0x2CE02  mov     esi, 1
0x2CE07  jmp     loc_2CD73
0x2CE0C  mov     eax, esi
0x2CE0E  jmp     short loc_2CE12
0x2CE10  xor     eax, eax
0x2CE12  add     esp, 10h
0x2CE15  pop     ebp
0x2CE16  pop     edi
0x2CE17  pop     esi
0x2CE18  pop     ebx
0x2CE19  retn
