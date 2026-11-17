#!/usr/bin/env python3
"""Drive the OS in QEMU through the monitor socket and capture screenshots.

Usage: vm_shot.py <iso> <outdir> [script...]
Each script step is one of:
    wait:<seconds>
    key:<qemu sendkey string>     e.g. key:s-t-a-r-t-g-u-i or key:ret
    shot:<name>                   screendump -> <outdir>/<name>.ppm (+ .png)
    log:<text>                    wait until <text> appears in the serial log
"""
import os, socket, subprocess, sys, time

SOCK = '/tmp/md_qmon.sock'
SER = '/tmp/md_serial.log'


def connect(retries=120):
    for _ in range(retries):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(SOCK)
            s.settimeout(5)
            return s
        except OSError:
            time.sleep(0.25)
    raise RuntimeError('monitor socket never appeared')


def mon(s, cmd):
    s.sendall((cmd + '\n').encode())
    time.sleep(0.15)
    try:
        return s.recv(65536).decode('utf-8', 'replace')
    except socket.timeout:
        return ''


def ppm_to_png(src, dst):
    data = open(src, 'rb').read()
    if not data.startswith(b'P6'):
        raise RuntimeError('not a P6 ppm: ' + src)
    # parse header
    fields, idx = [], 2
    while len(fields) < 3:
        while idx < len(data) and data[idx:idx + 1].isspace():
            idx += 1
        if data[idx:idx + 1] == b'#':
            while data[idx:idx + 1] != b'\n':
                idx += 1
            continue
        start = idx
        while idx < len(data) and not data[idx:idx + 1].isspace():
            idx += 1
        fields.append(int(data[start:idx]))
    idx += 1
    w, h, _maxv = fields
    pix = data[idx:idx + w * h * 3]
    import struct, zlib
    raw = b''.join(b'\x00' + pix[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, payload):
        return (struct.pack('>I', len(payload)) + tag + payload +
                struct.pack('>I', zlib.crc32(tag + payload) & 0xffffffff))

    png = (b'\x89PNG\r\n\x1a\n' +
           chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
           chunk(b'IDAT', zlib.compress(raw, 6)) +
           chunk(b'IEND', b''))
    open(dst, 'wb').write(png)
    return w, h


def main():
    iso, outdir = sys.argv[1], sys.argv[2]
    steps = sys.argv[3:]
    os.makedirs(outdir, exist_ok=True)
    for f in (SOCK, SER):
        if os.path.exists(f):
            os.unlink(f)

    qemu = subprocess.Popen([
        'qemu-system-i386', '-cdrom', iso, '-m', '256M',
        '-display', 'none',
        '-serial', 'file:' + SER,
        '-monitor', 'unix:%s,server,nowait' % SOCK,
        '-netdev', 'user,id=net0', '-device', 'rtl8139,netdev=net0',
        '-device', 'isa-debug-exit,iobase=0x501,iosize=0x04',
    ], stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

    try:
        s = connect()
        mon(s, '')
        for step in steps:
            kind, _, arg = step.partition(':')
            if kind == 'wait':
                time.sleep(float(arg))
            elif kind == 'log':
                deadline = time.time() + 90
                while time.time() < deadline:
                    try:
                        txt = open(SER, 'r', errors='replace').read()
                    except OSError:
                        txt = ''
                    if arg in txt:
                        break
                    time.sleep(0.5)
                else:
                    print('WARN: never saw %r' % arg)
            elif kind == 'mon':
                print(mon(s, arg))
            elif kind == 'key':
                mon(s, 'sendkey ' + arg)
            elif kind == 'shot':
                ppm = os.path.join(outdir, arg + '.ppm')
                mon(s, 'screendump ' + ppm)
                time.sleep(0.6)
                if os.path.exists(ppm):
                    w, h = ppm_to_png(ppm, os.path.join(outdir, arg + '.png'))
                    print('shot %s (%dx%d)' % (arg, w, h))
                else:
                    print('shot %s FAILED' % arg)
            else:
                raise SystemExit('bad step ' + step)
    finally:
        try:
            mon(s, 'quit')
        except Exception:
            pass
        time.sleep(0.5)
        if qemu.poll() is None:
            qemu.kill()
        try:
            os.unlink(SOCK)
        except OSError:
            pass


if __name__ == '__main__':
    main()
