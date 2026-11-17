import io
p = 'D:/md141.raw'
d = open(p,'rb').read()
print('size', len(d))
# MBR partition entries
for i in range(4):
    e = d[0x1be+i*16:0x1ce+i*16]
    lba = int.from_bytes(e[8:12],'little'); sz = int.from_bytes(e[12:16],'little')
    print('part',i,'type',hex(e[4]),'lba',lba,'size',sz)
    if e[4] in (0x0e,0x06,0x0b,0x0c) and lba:
        b = d[lba*512:lba*512+64]
        bps = int.from_bytes(b[11:13],'little'); spc = b[13]
        res = int.from_bytes(b[14:16],'little'); nf = b[16]
        re_ = int.from_bytes(b[17:19],'little'); fatsz = int.from_bytes(b[22:24],'little')
        print('  FAT BPB bps',bps,'spc',spc,'res',res,'nfats',nf,'rootent',re_,'fatsz',fatsz)
        print('  oem',b[3:11])
        rootlba = lba + res + nf*fatsz
        root = d[rootlba*512:rootlba*512+512]
        print('  root first 96 bytes:', root[:96].hex())
