import io, re

root = '/mnt/d/Varsayılan Proje/minimal-desktop'
src = io.open(root + '/kernel/shell.c', encoding='utf-8').read()

rows = []
rx = re.compile(r'\{\s*"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*cmd_\w+\s*\}')
for m in rx.finditer(src):
    rows.append((m.group(1), m.group(2), m.group(3), m.group(4)))

print('commands found:', len(rows))
groups = {}
for name, group, usage, desc in rows:
    groups.setdefault(group, []).append((name, usage, desc))

out = ['## Complete command reference', '',
       'Every command registered in `kernel/shell.c` (%d total), grouped by category.' % len(rows), '']
for group in sorted(groups):
    out.append('### ' + group)
    out.append('')
    out.append('| Command | Usage | Description |')
    out.append('|---|---|---|')
    for name, usage, desc in sorted(groups[group]):
        u = usage.replace('|', '\\|') or '—'
        d = desc.replace('|', '\\|') or '—'
        out.append('| `%s` | `%s` | %s |' % (name, u, d))
    out.append('')

out += [
    '### Keyboard / GUI shortcuts',
    '',
    '| Key | Action |',
    '|---|---|',
    '| `Alt+Shift` | cycle keyboard layouts (21 layouts) |',
    '| `Super` | open/close the start menu |',
    '| `Alt+Tab` | cycle windows |',
    '| `Alt+Enter` | open a GUI terminal |',
    '| `Ctrl+PrtSc` | save a BMP screenshot into RAMFS |',
    '| `Esc` | close the active GUI window / leave Miniland |',
    '| `F1` | toggle PC speaker mute |',
    '| `Ctrl+Alt+Del` | reboot |',
    '',
    '### Boot flags',
    '',
    '| Flag | Meaning |',
    '|---|---|',
    '| `--autotest` | run the 18-check self test, report on COM1, exit via ISA debug port 0x501 |',
    '| `--safe` | skip store install and network activity |',
    '| `--installtest` | run the disk installer without the confirmation prompt |',
    '',
]

readme = io.open(root + '/README.md', encoding='utf-8').read()
if '## Complete command reference' in readme:
    readme = readme.split('## Complete command reference')[0].rstrip() + '\n\n'
io.open(root + '/README.md', 'w', encoding='utf-8').write(readme + '\n'.join(out))
print('README updated with', len(rows), 'commands')
