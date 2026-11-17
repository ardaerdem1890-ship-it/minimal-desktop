#!/usr/bin/env python3
"""Publish the project to GitHub with the device-flow token: create the repo,
cut a release for every version and upload its ISO.

Run after tools/gh_login.py has written build/gh_token.txt.
"""
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOKEN = open(os.path.join(ROOT, 'build', 'gh_token.txt')).read().strip()
OWNER = 'ardaerdem1890-ship-it'
REPO = 'minimal-desktop'
API = 'https://api.github.com'


def request(url, data=None, method=None, raw=None, content_type=None):
    headers = {
        'Authorization': 'Bearer ' + TOKEN,
        'Accept': 'application/vnd.github+json',
        'User-Agent': 'minimal-desktop-publisher',
        'X-GitHub-Api-Version': '2022-11-28',
    }
    body = None
    if data is not None:
        body = json.dumps(data).encode()
        headers['Content-Type'] = 'application/json'
    if raw is not None:
        body = raw
        headers['Content-Type'] = content_type or 'application/octet-stream'
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=180) as r:
            text = r.read().decode('utf-8', 'replace')
            return r.status, json.loads(text) if text else None
    except urllib.error.HTTPError as e:
        text = e.read().decode('utf-8', 'replace')
        try:
            parsed = json.loads(text)
        except Exception:
            parsed = text
        return e.code, parsed


def ensure_repo():
    code, res = request(API + '/repos/' + OWNER + '/' + REPO)
    if code == 200:
        print('repo exists:', res['html_url'])
        return
    code, res = request(API + '/user/repos', {
        'name': REPO,
        'description': 'A from-scratch 32-bit x86 hobby OS: GRUB Multiboot2, VGA GUI, '
                       'shell, package store, networking and a disk installer.',
        'homepage': 'https://github.com/' + OWNER + '/' + REPO,
        'private': False,
        'has_issues': True,
        'has_projects': False,
        'has_wiki': False,
    })
    if code not in (200, 201):
        raise SystemExit('repo creation failed: %s %s' % (code, res))
    print('repo created:', res['html_url'])


def ensure_release(tag, name, body):
    code, res = request(API + '/repos/%s/%s/releases/tags/%s' % (OWNER, REPO, tag))
    if code == 200:
        print('release exists:', tag)
        return res
    code, res = request(API + '/repos/%s/%s/releases' % (OWNER, REPO), {
        'tag_name': tag, 'name': name, 'body': body or '',
        'draft': False, 'prerelease': False,
    })
    if code not in (200, 201):
        raise SystemExit('release failed for %s: %s %s' % (tag, code, res))
    print('release created:', tag, res.get('html_url'))
    return res


def upload_asset(release, path, label):
    fname = os.path.basename(path)
    existing = release.get('assets', [])
    if any(a['name'] == fname for a in existing):
        print('  asset already present:', fname)
        return
    url = release['upload_url'].split('{')[0] + '?name=' + urllib.parse.quote(fname)
    size = os.path.getsize(path)
    print('  uploading %s (%d bytes)' % (fname, size))
    with open(path, 'rb') as f:
        code, res = request(url, raw=f.read(),
                            content_type='application/octet-stream')
    if code not in (200, 201):
        raise SystemExit('asset upload failed: %s %s' % (code, res))
    print('  ok:', res.get('browser_download_url'))


def main():
    ensure_repo()
    if '--repo-only' in sys.argv:
        return

    versions = json.load(open(os.path.join(ROOT, 'website', 'downloads', 'versions.json'),
                              encoding='utf-8-sig'))
    for v in versions:
        ver = v['version']
        tag = 'v' + ver
        path = os.path.join(ROOT, 'website', v['file'].replace('/', os.sep))
        if not os.path.exists(path):
            print('SKIP %s: missing %s' % (tag, path))
            continue
        release = ensure_release(tag, v.get('label') or tag, v.get('notes', ''))
        upload_asset(release, path, v.get('label'))
    print('done')


if __name__ == '__main__':
    main()
