#!/usr/bin/env python3
"""GitHub device-flow login used by the publishing step.

Writes the one-time code to /tmp/gh_device.txt as soon as GitHub issues it so
the agent can hand it to the user, then polls until the account is authorised
and stores the token in /tmp/gh_token.txt (mode 600).
"""
import json
import os
import time
import urllib.parse
import urllib.request

CLIENT_ID = "178c6fc778ccc68e1d6a"      # GitHub CLI public client id
SCOPES = "repo read:user user:email workflow"
BUILD = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'build')
DEVICE_FILE = os.path.join(BUILD, 'gh_device.txt')
TOKEN_FILE = os.path.join(BUILD, 'gh_token.txt')


def post(url, data):
    req = urllib.request.Request(
        url,
        data=urllib.parse.urlencode(data).encode(),
        headers={"Accept": "application/json",
                 "User-Agent": "minimal-desktop-publisher"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode())


def main():
    if os.path.exists(TOKEN_FILE):
        os.unlink(TOKEN_FILE)

    dev = post("https://github.com/login/device/code",
               {"client_id": CLIENT_ID, "scope": SCOPES})
    code = dev["user_code"]
    verify = dev.get("verification_uri", "https://github.com/login/device")
    with open(DEVICE_FILE, "w") as f:
        f.write("%s\n%s\n" % (code, verify))
    print("CODE %s  URL %s" % (code, verify), flush=True)

    interval = int(dev.get("interval", 5))
    deadline = time.time() + int(dev.get("expires_in", 900))
    while time.time() < deadline:
        time.sleep(interval)
        res = post("https://github.com/login/oauth/access_token", {
            "client_id": CLIENT_ID,
            "device_code": dev["device_code"],
            "grant_type": "urn:ietf:params:oauth:grant-type:device_code",
        })
        err = res.get("error")
        if err == "authorization_pending":
            continue
        if err == "slow_down":
            interval += 5
            continue
        if err:
            print("FAILED: %s (%s)" % (err, res.get("error_description", "")))
            return 1
        token = res.get("access_token")
        if not token:
            print("FAILED: no access_token")
            return 1
        fd = os.open(TOKEN_FILE, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        os.write(fd, token.encode())
        os.close(fd)
        print("TOKEN saved")
        return 0
    print("FAILED: expired")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
