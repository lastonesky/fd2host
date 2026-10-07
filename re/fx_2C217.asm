0x2C217  push    54h ; 'T'
0x2C21C  call    sub_3702F
0x2C221  push    ebx
0x2C222  push    esi
0x2C223  push    edi
0x2C224  push    ebp
0x2C225  sub     esp, 2Ch
0x2C228  xor     ebp, ebp
0x2C22A  mov     ecx, 0Ah
0x2C22F  mov     edi, esp
0x2C231  mov     esi, offset unk_525B5
0x2C236  rep movsd
0x2C238  mov     edx, [esp+3Ch+arg_0]
0x2C23C  mov     eax, edx
0x2C23E  shl     eax, 2
0x2C241  add     edx, eax
0x2C243  shl     edx, 4
0x2C246  mov     eax, dword_53A45
0x2C24B  movzx   eax, byte ptr [edx+eax+6]
0x2C250  test    eax, eax
0x2C252  jnz     short loc_2C270
0x2C254  mov     [esp+3Ch+var_14], ebp
0x2C258  jmp     short loc_2C269
0x2C25A  mov     eax, [esp+3Ch+var_14]
0x2C25E  add     [esp+eax*4+3Ch+var_3C], 8Fh
0x2C265  inc     [esp+3Ch+var_14]
0x2C269  cmp     [esp+3Ch+var_14], 0Ah
0x2C26E  jl      short loc_2C25A
0x2C270  movzx   eax, [esp+3Ch+arg_10]
0x2C275  test    eax, eax
0x2C277  jnz     short loc_2C2E0
0x2C279  mov     [esp+3Ch+var_14], eax
0x2C27D  jmp     short loc_2C2BF
0x2C27F  mov     edx, [esp+3Ch+var_14]
0x2C283  neg     edx
0x2C285  add     edx, edx
0x2C287  mov     eax, [esp+3Ch+var_14]
0x2C28B  mov     dword_54018[eax*4], edx
0x2C292  mov     dword_54030[eax*4], eax
0x2C299  call    sub_4EBE3
0x2C29E  mov     edx, eax
0x2C2A0  mov     ebx, 2
0x2C2A5  sar     edx, 1Fh
0x2C2A8  idiv    ebx
0x2C2AA  mov     eax, edx
0x2C2AC  shl     eax, 3
0x2C2AF  sub     eax, edx
0x2C2B1  mov     edx, [esp+3Ch+var_14]
0x2C2B5  mov     byte_54048[edx], al
0x2C2BB  inc     [esp+3Ch+var_14]
0x2C2BF  cmp     [esp+3Ch+var_14], 6
0x2C2C4  jl      short loc_2C27F
0x2C2C6  mov     al, byte ptr [esp+3Ch+var_14]
0x2C2CA  mov     byte_5404E, al
0x2C2CF  mov     byte_5404F, 0
0x2C2D6  mov     eax, 2
0x2C2DB  jmp     loc_2C439
0x2C2E0  cmp     eax, 3
0x2C2E3  jnz     short loc_2C2EF
0x2C2E5  mov     eax, 0Ch
0x2C2EA  jmp     loc_2C439
0x2C2EF  cmp     eax, 6
0x2C2F2  jnz     short loc_2C305
0x2C2F4  mov     byte_5404F, 1
0x2C2FB  mov     eax, 8
0x2C300  jmp     loc_2C439
0x2C305  cmp     eax, 2
0x2C308  jz      short loc_2C318
0x2C30A  cmp     eax, 5
0x2C30D  jz      short loc_2C318
0x2C30F  cmp     eax, 8
0x2C312  jnz     loc_2C437
0x2C318  mov     [esp+3Ch+var_14], 0
0x2C320  jmp     short loc_2C326
0x2C322  inc     [esp+3Ch+var_14]
0x2C326  cmp     [esp+3Ch+var_14], 6
0x2C32B  jge     loc_2C433
0x2C331  mov     eax, [esp+3Ch+var_14]
0x2C335  shl     eax, 2
0x2C338  cmp     dword_54018[eax], 0
0x2C33F  jl      short loc_2C37D
0x2C341  cmp     dword_54018[eax], 7
0x2C348  jge     short loc_2C37D
0x2C34A  push    0FFFFFFFFh
0x2C34C  push    [esp+40h+arg_C]
0x2C350  mov     edx, dword_54030[eax]
0x2C356  mov     ebx, [esp+44h+arg_8]
0x2C35A  add     ebx, [esp+edx*4+44h+var_3C]
0x2C35E  push    ebx
0x2C35F  mov     edx, [esp+48h+var_14]
0x2C363  movzx   edx, byte_54048[edx]
0x2C36A  add     edx, dword_54018[eax]
0x2C370  push    edx
0x2C371  push    [esp+4Ch+arg_4]
0x2C375  call    sub_2EB9F
0x2C37A  add     esp, 14h
0x2C37D  mov     eax, [esp+3Ch+var_14]
0x2C381  cmp     dword_54018[eax*4], 0
0x2C389  jnz     short loc_2C39D
0x2C38B  push    1
0x2C38D  push    1
0x2C38F  push    dword_54153
0x2C395  call    sub_25A96
0x2C39A  add     esp, 0Ch
0x2C39D  mov     eax, [esp+3Ch+var_14]
0x2C3A1  inc     dword_54018[eax*4]
0x2C3A8  cmp     dword_54018[eax*4], 3
0x2C3B0  jnz     short loc_2C3B7
0x2C3B2  mov     ebp, 1
0x2C3B7  mov     ebx, [esp+3Ch+var_14]
0x2C3BB  shl     ebx, 2
0x2C3BE  cmp     dword_54018[ebx], 8
0x2C3C5  jnz     loc_2C322
0x2C3CB  movzx   eax, byte_5404F
0x2C3D2  test    eax, eax
0x2C3D4  jnz     loc_2C322
0x2C3DA  inc     byte_5404E
0x2C3E0  movzx   edx, byte_5404E
0x2C3E7  mov     ecx, 0Ah
0x2C3EC  mov     eax, edx
0x2C3EE  sar     edx, 1Fh
0x2C3F1  idiv    ecx
0x2C3F3  mov     byte_5404E, dl
0x2C3F9  movzx   eax, dl
0x2C3FC  mov     dword_54030[ebx], eax
0x2C402  mov     dword_54018[ebx], 0
0x2C40C  call    sub_4EBE3
0x2C411  mov     edx, eax
0x2C413  mov     ebx, 2
0x2C418  sar     edx, 1Fh
0x2C41B  idiv    ebx
0x2C41D  mov     eax, edx
0x2C41F  shl     eax, 3
0x2C422  sub     eax, edx
0x2C424  mov     edx, [esp+3Ch+var_14]
0x2C428  mov     byte_54048[edx], al
0x2C42E  jmp     loc_2C322
0x2C433  mov     eax, ebp
0x2C435  jmp     short loc_2C439
0x2C437  xor     eax, eax
0x2C439  add     esp, 2Ch
0x2C43C  pop     ebp
0x2C43D  pop     edi
0x2C43E  pop     esi
0x2C43F  pop     ebx
0x2C440  retn
