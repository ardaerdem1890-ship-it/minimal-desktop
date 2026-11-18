import subprocess, re, json, os, urllib.request
url=subprocess.check_output(['git','remote','get-url','origin'],text=True).strip()
m=re.match(r'https://x-access-token:([^@]+)@',url)
if not m: raise SystemExit('no token')
token=m.group(1); owner='ardaerdem1890-ship-it'; repo='minimal-desktop'
def req(u,data=None,method=None,raw=None,ct=None):
    h={'Authorization':'Bearer '+token,'User-Agent':'md','Accept':'application/vnd.github+json'}
    b=None
    if data is not None: b=json.dumps(data).encode(); h['Content-Type']='application/json'
    if raw is not None: b=raw; h['Content-Type']=ct or 'application/octet-stream'
    r=urllib.request.Request(u,data=b,headers=h,method=method)
    with urllib.request.urlopen(r,timeout=180) as resp:
        t=resp.read().decode('utf-8','replace'); return json.loads(t) if t else None
print('account:',req('https://api.github.com/user')['login'])
v=json.load(open('website/downloads/versions.json',encoding='utf-8-sig'))[0]
tag='v'+v['version']; print('version:',tag)
try:
    rel=req('https://api.github.com/repos/%s/%s/releases/tags/%s'%(owner,repo,tag))
    for a in rel.get('assets',[]):
        req('https://api.github.com/repos/%s/%s/releases/assets/%d'%(owner,repo,a['id']),method='DELETE')
except Exception:
    rel=req('https://api.github.com/repos/%s/%s/releases'%(owner,repo),
        {'tag_name':tag,'name':v['label'],'body':v.get('notes',''),'draft':False,'prerelease':False})
    print('created:',rel['html_url'])
path='website/'+v['file']
a=req(rel['upload_url'].split('{')[0]+'?name='+os.path.basename(path),raw=open(path,'rb').read(),ct='application/octet-stream')
print('uploaded:',a['browser_download_url'],a['size'])
