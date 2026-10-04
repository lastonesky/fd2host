int __cdecl main(int argc, const char **argv, const char **envp)
{
  __int64 v3; // rax
  const char *v4; // edi
  int v5; // ebp
  int v6; // ebx
  int v7; // esi
  int v9; // ebp
  int v10; // eax
  int v11; // ebp
  int v12; // ebx
  void *v13; // eax
  unsigned int v14; // [esp+0h] [ebp-1Ch]
  int v15; // [esp+4h] [ebp-18h]
  int v16; // [esp+8h] [ebp-14h]

  sub_10840(15);
  sub_1225E(1);
  v4 = argv[1];
  LODWORD(v3) = fopen(argv[2], &unk_30004);
  v5 = v3;
  if ( (_DWORD)v3 == 0 )
    return sub_10900(v3, HIDWORD(v3));
  v3 = filelength(*(_DWORD *)(v3 + 16));
  v6 = v3;
  LODWORD(v3) = malloc(v3);
  v7 = v3;
  v14 = v3;
  if ( v6 == 0 )
  {
    LODWORD(v3) = fclose(v5);
    return sub_10900(v3, HIDWORD(v3));
  }
  if ( fread(v3, 1, v6, v5) != v6 )
  {
    free(v7);
    LODWORD(v3) = fclose(v5);
    return sub_10900(v3, HIDWORD(v3));
  }
  fclose(v5);
  LODWORD(v3) = operator new(0x15Du);
  if ( (_DWORD)v3 != 0 )
    LODWORD(v3) = sub_10420(v3, v4);
  v9 = v3;
  v16 = v3;
  if ( (_DWORD)v3 != 0 )
  {
    if ( sub_104D0(v3) == -1 )
    {
      LODWORD(v3) = free(v14);
      if ( v9 == 0 )
        return sub_10900(v3, HIDWORD(v3));
    }
    else
    {
      sub_12206(*(_DWORD *)(v9 + 341));
      sub_10B80(v14, 1, -1, -1);
      v10 = *(unsigned __int16 *)(v9 + 317) - 1;
      v11 = 0;
      v15 = v10;
      if ( v10 > 0 )
      {
        while ( 1 )
        {
          v12 = dword_32700;
          qmemcpy(MEMORY[0xA0000], *(const void **)(v16 + 337), sizeof(MEMORY[0xA0000]));
          if ( sub_10770(v16) == -1 )
            break;
          while ( v12 == dword_32700 )
            printf(&unk_30007, v14);
          if ( sub_12247() == 0 && ++v11 < v15 )
            continue;
          goto LABEL_21;
        }
        v3 = __PAIR64__(v14, free(v14));
        if ( v16 == 0 )
          return sub_10900(v3, HIDWORD(v3));
        v13 = (void *)sub_10470(v16);
        goto LABEL_23;
      }
LABEL_21:
      LODWORD(v3) = free(v14);
      v9 = v16;
      if ( v16 == 0 )
        return sub_10900(v3, HIDWORD(v3));
    }
    v13 = (void *)sub_10470(v9);
LABEL_23:
    operator delete(v13);
  }
  return sub_10900(v3, HIDWORD(v3));
}
