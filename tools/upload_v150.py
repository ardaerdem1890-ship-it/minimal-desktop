import json, os, urllib.request

token = open('build/gh_token.txt').read().strip()
owner='ardaerdem1890-ship-it'; repo='minimal-desktop'
def req(url, data=None, method=None, raw=None, ct=None):
    h={'Authorization':'Bearer '+token,'User-Agent':'md','Accept':'application/vnd.github+json'}
    b=None
    if data is not None:
        b=json.dumps(data).encode(); h['Content-Type']='application/json'
    if raw is not None:
        b=raw; h['Content-Type']=ct or 'application/octet-stream'
    r=urllib.request.Request(url,data=b,headers=h,method=method)
    with urllib.request.urlopen(r,timeout=180) as resp:
        t=resp.read().decode('utf-8','replace')
        return json.loads(t) if t else None

# verify identity
u=req('https://api.github.com/user')
print('account:', u['login'])

v=json.load(open('website/downloads/versions.json',encoding='utf-8-sig'))[0]
tag='v'+v['version']
print('version:', v['version'])
try:
    rel=req('https://api.github.com/repos/%s/%s/releases/tags/%s'%(owner,repo,tag))
    print('release exists:', rel['html_url'])
    # delete stale asset if present
    for a in rel.get('assets',[]):
        req('https://api.github.com/repos/%s/%s/releases/assets/%d'%(owner,repo,a['id']),method='DELETE')
        print('deleted stale asset',a['name'])
except Exception:
    rel=req('https://api.github.com/repos/%s/%s/releases'%(owner,repo),{
        'tag_name':tag,'name':v['label'],'body':v.get('notes',''),'draft':False,'prerelease':False})
    print('release created:', rel['html_url'])

path='website/'+v['file']
data=open(path,'rb').read()
url=rel['upload_url'].split('{')[0]+'?name='+os.path.basename(path)
a=req(url,raw=data,ct='application/octet-stream')
print('uploaded:', a['browser_download_url'], a['size'])
