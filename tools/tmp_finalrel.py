import io, json, os, shutil, hashlib
root='/mnt/d/Varsayılan Proje/minimal-desktop'
iso=os.path.join(root,'build','minimal_desktop.iso')
size=os.path.getsize(iso); h=hashlib.sha256(open(iso,'rb').read()).hexdigest()
wd=os.path.join(root,'website','downloads')
shutil.copyfile(iso,os.path.join(wd,'minimal_desktop_v1.4.1.iso'))
p=os.path.join(wd,'versions.json'); d=json.load(io.open(p,encoding='utf-8-sig'))
for v in d:
    if v['version']=='1.4.1':
        v['sha256']=h; v['size']=size
        v['notes']=v['notes'].rstrip('. ')+'. Fixed: FAT disk persistence (files under /disk now survive reboot), VirtualBox text-mode font corruption after selftest, e1000 DHCP quirk workaround via PCnet.'
io.open(p,'w',encoding='utf-8').write(json.dumps(d,indent=2,ensure_ascii=False)+'\n')
print('v1.4.1',size,h)
