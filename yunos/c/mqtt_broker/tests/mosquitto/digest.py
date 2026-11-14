#!/usr/bin/env python3
#
#   Digest of one test output: test lines + broker WARNING/ERROR msgs
#
import sys, re, json
for path in sys.argv[1:]:
    print("=====", path.split('/')[-1])
    txt = open(path, errors='replace').read()
    txt = re.sub(r'\x1b\[[0-9;]*m', '', txt)
    out = []
    for line in txt.splitlines():
        if re.match(r'^(INFO|DEBUG|WARNING|ERROR|CRITICAL|ALERT|EMERG|NOTICE): \{', line):
            lvl = line.split(':')[0]
            if lvl in ('INFO', 'DEBUG'):
                continue
            m = re.search(r'"msg": "([^"]*)"', line)
            g = re.search(r'"gclass": "([^"]*)"', line)
            f = re.search(r'"function": "([^"]*)"', line)
            out.append("  [%s] %s %s: %s" % (lvl, g.group(1) if g else '', f.group(1) if f else '', m.group(1) if m else line[:200]))
            continue
        if line.strip().startswith('File') and '/broker/' in line:
            out.append("  @" + line.strip().split('/broker/')[1])
            continue
        if line.startswith((' ', '{', '}', '\t')) or not line.strip():
            continue
        if line.startswith(('MosquittoBroker', 'STANDIN: listener')):
            continue
        out.append(line[:400])
    # dedupe consecutive
    prev = None
    n = 0
    for l in out:
        if l == prev:
            continue
        prev = l
        print(l)
        n += 1
        if n > 60:
            print("...")
            break
