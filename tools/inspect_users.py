import os
d=open('/mnt/d/md170_check.raw','rb').read()
print('size',len(d))
for i in range(4):
    e=d[0x1be+i*16:0x1ce+i*16]
    lba=int.from_bytes(e[8:12],'little'); sz=int.from_bytes(e[12:16],'little')
    print('part',i,'type',hex(e[4]),'lba',lba,'size',sz)
    if e[4] in (0x0e,0x06,0x0b,0x0c) and lba:
        b=d[lba*512:lba*512+64]
        fatsz=int.from_bytes(b[22:24],'little'); nf=b[16]; res=int.from_bytes(b[14:16],'little')
        rootent=int.from_bytes(b[17:19],'little'); rootsz=(rootent*32+511)//512
        rootlba=lba+res+nf*fatsz
        root=d[rootlba*512:rootlba*512+rootsz*512]
        print('  root entries:')
        for j in range(8):
            e2=root[j*32:(j+1)*32]
            if e2[0]==0: break
            print('   ',repr(e2[:11]),'cluster',int.from_bytes(e2[26:28],'little'),'size',int.from_bytes(e2[28:32],'little'))
