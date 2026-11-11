#!/usr/bin/env python3
"""
Check that no two test binaries use the same fixed port.

ctest -j runs test binaries at once, and two that listen on one port fail
with "bind() FAILED". Every port a test writes (a url, a *PORT* define, a
"port" key, htons()) is listed under the tests that use it, across
tests/c and performance/c (ctest runs both).

The files of ONE binary may share a port: `main_<name>.c` with its
`c_<name>.c` / `c_<name>.h`, and the files under `<test>/src/`. Anything else
sharing a port is a collision.

    python3 scripts/check_test_ports.py            # exit 1 on a collision, or a file not read
    python3 scripts/check_test_ports.py --list     # every port and its tests

A port built at compile time (a -DPORT from CMake, as c_task_authenticate
does) is not seen here: keep such ranges apart by hand.

A test whose `run_test.sh` runs it in a network namespace of its own
(`unshare` with `-n` or a bare `--net`, as c_agent_sandbox does) has a
loopback nobody else sees: its ports are not listed (the agent's fixed 1991
runs in two of them). Only a command line counts: not a comment, not
`--net=<file>` (a namespace that already exists, maybe shared), and not the
`unshare ... true` that probes whether namespaces are allowed.
"""
import collections
import os
import re
import shlex
import sys

ROOTS = ['tests/c', 'performance/c']
# Short options of unshare(1) that take a value in the next word
UNSHARE_ARG_OPTS = set('SGRw')
EXTENSIONS = ('.c', '.h', '.json', '.py', '.sh')
PATTERNS = [
    re.compile(r'[a-z]+s?://[^\s\'"/]*?:(\d{3,5})\b'),
    re.compile(r'#define\s+\w*PORT\w*\s+"?(\d{3,5})"?'),
    re.compile(r'htons\(\s*(\d{3,5})\s*\)'),
    re.compile(r'[\'"]\w*port\w*[\'"]\s*:\s*[\'"]?(\d{3,5})\b', re.I),
    re.compile(r'\bport\s*=\s*(\d{3,5})\b', re.I),
]


def binary_of(path):
    """The test binary a file belongs to, as a key."""
    parts = path.split(os.sep)
    if 'src' in parts:
        return os.sep.join(parts[:parts.index('src')])
    d, f = os.path.split(path)
    stem = os.path.splitext(f)[0]
    for prefix in ('main_', 'c_'):
        if stem.startswith(prefix):
            stem = stem[len(prefix):]
            break
    return os.path.join(d, stem)


def unshare_runs_in_new_netns(words):
    """Whether an unshare command (its words after `unshare`) runs a program
    in a NEW network namespace. The probe `unshare -n true` runs nothing."""
    new_netns = False
    i = 0
    while i < len(words):
        w = words[i]
        if w == '--':
            i += 1
            break
        if w.startswith('--'):
            if w == '--net':
                new_netns = True
            i += 1
            continue
        if w.startswith('-') and len(w) > 1:
            opts = w[1:]
            if 'n' in opts:
                new_netns = True
            if opts[-1] in UNSHARE_ARG_OPTS:
                i += 1
            i += 1
            continue
        break
    program = words[i] if i < len(words) else None
    return new_netns and program not in (None, 'true', ':')


def own_network_namespace(d):
    """Whether the test of dir d runs in a network namespace of its own."""
    try:
        with open(os.path.join(d, 'run_test.sh'), errors='replace') as fh:
            text = fh.read()
    except FileNotFoundError:
        return False
    for line in text.splitlines():
        try:
            words = shlex.split(line, comments=True)
        except ValueError:
            # an open quote: a command that spans lines, cut at the quote
            words = shlex.split(line.split('"')[0].split("'")[0], comments=True)
        if 'unshare' in words:
            if unshare_runs_in_new_netns(words[words.index('unshare') + 1:]):
                return True
    return False


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ports = collections.defaultdict(lambda: collections.defaultdict(set))
    unreadable = 0
    for root in ROOTS:
        top = os.path.join(base, root)
        for d, dirs, files in os.walk(top):
            dirs[:] = [x for x in dirs if x != 'build']
            if own_network_namespace(d):
                dirs[:] = []
                continue
            for f in files:
                if not f.endswith(EXTENSIONS):
                    continue
                p = os.path.join(d, f)
                rel = os.path.relpath(p, base)
                try:
                    with open(p, errors='replace') as fh:
                        s = fh.read()
                except OSError as e:
                    # A file not read is a port not checked: said, and a failure
                    print(f'cannot read {rel}: {e.strerror}', file=sys.stderr)
                    unreadable += 1
                    continue
                for pat in PATTERNS:
                    for m in pat.finditer(s):
                        port = int(m.group(1))
                        if 1024 <= port <= 65535:
                            ports[port][binary_of(rel)].add(rel)

    collisions = 0
    for port in sorted(ports):
        binaries = ports[port]
        if '--list' in sys.argv or len(binaries) > 1:
            mark = 'COLLISION' if len(binaries) > 1 else ''
            print(f'{port} {mark}'.rstrip())
            for b in sorted(binaries):
                print(f'    {b}: {" ".join(sorted(binaries[b]))}')
        if len(binaries) > 1:
            collisions += 1

    if collisions:
        print(f'{collisions} port(s) used by more than one test binary')
    if unreadable:
        print(f'{unreadable} file(s) could not be read: their ports are not checked')
    if collisions or unreadable:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
