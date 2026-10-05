#!/usr/bin/env python3
"""
Check that no two tests can use the same fixed directory at the same time.

ctest -j runs tests at once, and two that use one directory (a timeranger2
store under ~/tests_yuneta, a fixed /tmp path) collide: the second master of
a store opens as a replica, one test wipes what the other reads. Two tests
that name the same directory must be kept apart by ctest itself, in their
CMakeLists.txt. They are apart when:

    - they share a RESOURCE_LOCK,
    - one of them is RUN_SERIAL,
    - one DEPENDS on the other,
    - one is the FIXTURES_SETUP of a fixture the other FIXTURES_REQUIRED.

The directories of a test are read from the sources it is built from (and
the headers of tests/c or performance/c they include): a #define of a
*DATABASE* or *STORE* name, a build_path() segment after "tests_yuneta", a
literal tests_yuneta/<dir>, a literal /tmp/<path>; a directory and the ones
under it are one place. The properties are
ctest's own (--show-only=json-v1), so a configured and built tree is needed.

Tests that run the SAME executable with other arguments (the groups of
test_c_treedb_literal_wins) are not compared: the binary picks its
directories from its arguments, which this reading cannot see. A directory
built at run time from a format ("c_treedb_literal_wins_%s") is not seen
either.

    python3 scripts/check_test_databases.py                # exit 1 on a collision, or a test not read
    python3 scripts/check_test_databases.py --list         # every directory and its tests
    python3 scripts/check_test_databases.py --build-dir=X  # another build tree (default: build)

Sibling of scripts/check_test_ports.py, which does the same for ports.
"""
import collections
import json
import os
import re
import subprocess
import sys

ROOTS = ['tests/c', 'performance/c']
SCRIPT_RUNNERS = ('sh', 'bash', 'python', 'python3')
PATTERNS = [
    (re.compile(r'#define\s+\w*(?:DATABASE|STORE)\w*\s+"([^"%]+)"'), '~/tests_yuneta/'),
    (re.compile(r'"tests_yuneta"\s*,\s*"([^"%]+)"'), '~/tests_yuneta/'),
    (re.compile(r'tests_yuneta/([A-Za-z0-9_.-]+)'), '~/tests_yuneta/'),
    (re.compile(r'"(/tmp/[^"%]+)"'), ''),
]


def ctest_view(base, build_dir):
    """The tests as ctest sees them, or None (said)."""
    try:
        out = subprocess.run(
            ['ctest', '--test-dir', build_dir, '--show-only=json-v1'],
            cwd=base, capture_output=True, text=True, check=True
        ).stdout
        return json.loads(out).get('tests', [])
    except (OSError, subprocess.CalledProcessError, ValueError) as e:
        print(f'cannot read the tests of {build_dir} from ctest: {e}', file=sys.stderr)
        return None


def properties_of(test):
    props = {p['name']: p['value'] for p in test.get('properties', [])}

    def as_list(v):
        if v is None:
            return set()
        if isinstance(v, list):
            return set(v)
        return {v}

    return {
        'locks': as_list(props.get('RESOURCE_LOCK')),
        'serial': bool(props.get('RUN_SERIAL')),
        'depends': as_list(props.get('DEPENDS')),
        'setup': as_list(props.get('FIXTURES_SETUP')),
        'required': as_list(props.get('FIXTURES_REQUIRED')),
    }


def sources_of(base, build_abs, command):
    """The files a test is made of: its sources and the local headers they include."""
    exe = command[0]
    if os.path.basename(exe) in SCRIPT_RUNNERS and len(command) > 1:
        return [command[1]]
    if exe.endswith(('.sh', '.py')):
        return [exe]
    target_dir = os.path.join(os.path.dirname(exe), 'CMakeFiles', os.path.basename(exe) + '.dir')
    depend_info = os.path.join(target_dir, 'DependInfo.cmake')
    try:
        with open(depend_info) as fh:
            info = fh.read()
    except OSError:
        return None
    roots = tuple(os.path.join(base, r) + os.sep for r in ROOTS)
    files = set()
    for src, dep in re.findall(r'"(/[^"]+\.c)"\s+"[^"]+"\s+"\w+"\s+"([^"]+\.d)"', info):
        files.add(src)
        try:
            with open(os.path.join(build_abs, dep)) as fh:
                deps = fh.read()
        except OSError:
            continue
        for h in re.findall(r'(/\S+\.h)\b', deps):
            if h.startswith(roots):
                files.add(h)
    return sorted(files)


def directories_in(text):
    dirs = set()
    for pat, prefix in PATTERNS:
        for m in pat.finditer(text):
            dirs.add(prefix + m.group(1).rstrip('/'))
    return dirs


def kept_apart(a, b):
    if a['locks'] & b['locks']:
        return True
    if a['serial'] or b['serial']:
        return True
    if b['name'] in a['depends'] or a['name'] in b['depends']:
        return True
    if (a['setup'] & b['required']) or (b['setup'] & a['required']):
        return True
    return False


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build_dir = 'build'
    for arg in sys.argv[1:]:
        if arg.startswith('--build-dir='):
            build_dir = arg[len('--build-dir='):]
    build_abs = os.path.join(base, build_dir)

    tests = ctest_view(base, build_dir)
    if tests is None:
        return 1

    users = collections.defaultdict(list)    # directory -> [test]
    unread = 0
    for t in tests:
        command = t.get('command') or []
        if not command:
            print(f'{t["name"]}: ctest gives no command', file=sys.stderr)
            unread += 1
            continue
        files = sources_of(base, build_abs, command)
        if files is None:
            # A test not read is a directory not checked: said, and a failure
            print(f'{t["name"]}: cannot find the sources of {command[0]}', file=sys.stderr)
            unread += 1
            continue
        found = collections.defaultdict(set)
        for f in files:
            try:
                with open(f, errors='replace') as fh:
                    text = fh.read()
            except OSError as e:
                print(f'{t["name"]}: cannot read {f}: {e.strerror}', file=sys.stderr)
                unread += 1
                continue
            for d in directories_in(text):
                found[d].add(os.path.relpath(f, base))
        test = dict(properties_of(t), name=t['name'], exe=command[0])
        for d, where in found.items():
            users[d].append((test, sorted(where)))

    collisions = 0
    for d in sorted(users):
        # A directory and the ones under it are one place: their tests are compared
        merged = {}
        for d2 in users:
            if d2 == d or d2.startswith(d + '/'):
                for test, where in users[d2]:
                    if test['name'] in merged:
                        merged[test['name']] = (test, sorted(set(merged[test['name']][1]) | set(where)))
                    else:
                        merged[test['name']] = (test, where)
        entries = list(merged.values())
        pairs = []
        for i in range(len(entries)):
            for j in range(i + 1, len(entries)):
                a, b = entries[i][0], entries[j][0]
                if a['exe'] != b['exe'] and not kept_apart(a, b):
                    pairs.append(tuple(sorted((a['name'], b['name']))))
        if '--list' in sys.argv or pairs:
            print(f'{d} {"COLLISION" if pairs else ""}'.rstrip())
            for test, where in sorted(entries, key=lambda e: e[0]['name']):
                apart = sorted(test['locks'])
                if test['serial']:
                    apart.append('RUN_SERIAL')
                print(f'    {test["name"]}: {" ".join(where)}'
                      + (f'  [{", ".join(apart)}]' if apart else ''))
            for a, b in pairs:
                print(f'    can run at once: {a} <-> {b}')
        if pairs:
            collisions += 1

    if collisions:
        print(f'{collisions} directory(ies) that two tests can use at the same time: '
              'give them a shared RESOURCE_LOCK (or another name)')
    if unread:
        print(f'{unread} test(s) or file(s) could not be read: their directories are not checked')
    if collisions or unread:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
