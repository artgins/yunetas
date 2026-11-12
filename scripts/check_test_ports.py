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

A port built at compile time is read from the `target_compile_definitions()`
of a CMakeLists.txt (`TEST7_PORT_TLS="7798"`, `YUNETA_DNS_PORT=15353`), and
belongs to the target named there: a test twin built with another TLS
library from the same sources gets its ports this way. A port CMake computes
(`math(EXPR idp_port "18910 + ${n}")` then `IDP_PORT="${idp_port}"`) is not
in the source: it is read from the build tree, where CMake wrote it into
the `flags.make` of each target, and each target is judged on its own: a
target with no `flags.make` (a Ninja tree), a value that is not a port, a
target that defines some of the names and not the others, or a build tree
configured before the CMakeLists.txt last changed (a test just added to the
list has no target yet) is a failure like a file not read -- not checked is
never a pass.

    python3 scripts/check_test_ports.py --build-dir=X   # another build tree (default: build)

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
# Options of unshare(1) that take a value in the next word
UNSHARE_ARG_OPTS = set('SGRwl')
UNSHARE_LONG_ARG_OPTS = {
    '--propagation', '--setgroups', '--map-user', '--map-users',
    '--map-group', '--map-groups', '--owner', '--setuid', '--setgid',
    '--root', '--wd', '--load-interp', '--monotonic', '--boottime',
}
EXTENSIONS = ('.c', '.h', '.json', '.py', '.sh')
PATTERNS = [
    re.compile(r'[a-z]+s?://[^\s\'"/]*?:(\d{3,5})\b'),
    re.compile(r'#define\s+\w*PORT\w*\s+"?(\d{3,5})"?'),
    re.compile(r'htons\(\s*(\d{3,5})\s*\)'),
    re.compile(r'[\'"]\w*port\w*[\'"]\s*:\s*[\'"]?(\d{3,5})\b', re.I),
    re.compile(r'\bport\s*=\s*(\d{3,5})\b', re.I),
]
CMAKE_DEFINITIONS = re.compile(
    r'target_compile_definitions\(\s*([^\s)]+)([^)]*)\)', re.S)
CMAKE_PORT = re.compile(r'\b\w*PORT\w*\s*=\s*\\?"?(\d{3,5})\b')
CMAKE_COMPUTED_PORT = re.compile(r'\b(\w*PORT\w*)\s*=\s*\\?"?\$\{')
FLAGS_DEFINE = re.compile(r'-D(\w*PORT\w*)=(\S*)')


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
            if w in UNSHARE_LONG_ARG_OPTS:
                i += 1
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
    # a command continued with a trailing backslash is one line
    for line in text.replace('\\\n', ' ').splitlines():
        try:
            words = shlex.split(line, comments=True)
        except ValueError:
            # an open quote: a command that spans lines, cut at the quote
            words = shlex.split(line.split('"')[0].split("'")[0], comments=True)
        if 'unshare' in words:
            if unshare_runs_in_new_netns(words[words.index('unshare') + 1:]):
                return True
    return False


def computed_ports(build_dir, rel_dir, names, cmakelists):
    """The ports named `names` that CMake computed for the targets of the
    source dir rel_dir, read from the flags.make of each target in the build
    tree: ({(port, target)}, None), or (None, why) when they cannot all be
    read -- not checked is a failure, never a pass. Each target is judged
    on its own: a target with no flags.make (a tree of another generator,
    Ninja), a value that is not a port, or a target that defines some of
    the names and not the others; and so is the tree, configured before the
    CMakeLists.txt last changed (a test added to the list is not in it)."""
    cmake_files = os.path.join(build_dir, rel_dir, 'CMakeFiles')
    if not os.path.isdir(cmake_files):
        return None, 'not configured'
    check_cache = os.path.join(build_dir, 'CMakeFiles', 'cmake.check_cache')
    try:
        if os.path.getmtime(check_cache) < os.path.getmtime(cmakelists):
            return None, 'the build tree was configured before this CMakeLists.txt changed'
    except OSError:
        return None, f'no {check_cache}'
    found = set()
    problems = []
    for entry in sorted(os.listdir(cmake_files)):
        if not entry.endswith('.dir'):
            continue
        target = entry[:-len('.dir')]
        flags = os.path.join(cmake_files, entry, 'flags.make')
        try:
            with open(flags, errors='replace') as fh:
                text = fh.read()
        except FileNotFoundError:
            problems.append(f'{target} has no flags.make (not a Makefile tree?)')
            continue
        defined = {}
        for m in FLAGS_DEFINE.finditer(text):
            if m.group(1) in names:
                defined[m.group(1)] = m.group(2)
        if not defined:
            continue
        for name in sorted(names):
            value = defined.get(name)
            if value is None:
                problems.append(f'{target} lacks {name}')
                continue
            pm = re.fullmatch(r'\\?"?(\d{3,5})\\?"?', value)
            if not pm or not 1024 <= int(pm.group(1)) <= 65535:
                problems.append(f'{target}: {name}={value} is not a port')
                continue
            found.add((int(pm.group(1)), target))
    if problems:
        return None, '; '.join(problems)
    if not found:
        return None, f'{", ".join(sorted(names))} not found in any flags.make'
    return found, None


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build_dir = os.path.join(base, 'build')
    for arg in sys.argv[1:]:
        if arg.startswith('--build-dir='):
            build_dir = os.path.abspath(arg[len('--build-dir='):])
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
                if f == 'CMakeLists.txt':
                    p = os.path.join(d, f)
                    rel = os.path.relpath(p, base)
                    try:
                        with open(p, errors='replace') as fh:
                            s = fh.read()
                    except OSError as e:
                        print(f'cannot read {rel}: {e.strerror}', file=sys.stderr)
                        unreadable += 1
                        continue
                    rel_dir = os.path.relpath(d, base)
                    computed = set()
                    for m in CMAKE_DEFINITIONS.finditer(s):
                        target = os.path.join(rel_dir, m.group(1))
                        for pm in CMAKE_PORT.finditer(m.group(2)):
                            port = int(pm.group(1))
                            if 1024 <= port <= 65535:
                                ports[port][target].add(rel)
                        for pm in CMAKE_COMPUTED_PORT.finditer(m.group(2)):
                            computed.add(pm.group(1))
                    if computed:
                        found, why = computed_ports(build_dir, rel_dir, computed, p)
                        if found is None:
                            print(f'{rel}: {", ".join(sorted(computed))} computed by CMake, '
                                  f'not read from {os.path.join(build_dir, rel_dir)}: {why}; '
                                  f'configure and build the tests first', file=sys.stderr)
                            unreadable += 1
                            continue
                        for port, target in found:
                            if 1024 <= port <= 65535:
                                ports[port][os.path.join(rel_dir, target)].add(rel)
                    continue
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
