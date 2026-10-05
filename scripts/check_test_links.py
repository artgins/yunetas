#!/usr/bin/env python3
"""
Check that no build links an archive of the SDK by a bare name.

A test (or a yuno, a util) links the installed archives, and it is relinked
when one of them changes only if CMake knows the FILE: `${LIB_DEST_DIR}/libfoo.a`
(or a list of project.cmake, YUNETAS_KERNEL_LIBS and the like) is a
dependency of the link. A bare name is a `-l` search, and the binary keeps
running the old library after a change of the one it links. Up to 7.26.3,
18 tests named the kernel archives bare; `make clean`, and then the
reinstall of the root tree, hid it.

Read from every CMakeLists.txt of tests/c, performance/c, stress/c, yunos/c,
utils/c and modules/c, comments left out:

    - a token `libfoo.a` with no directory, anywhere (also in a set() that a
      target_link_libraries() uses later);
    - a token `-lfoo`, or a bare `foo` inside target_link_libraries(), when
      `libfoo.a` is an archive of outputs/lib or outputs_ext/lib (so system
      libraries such as `pthread` or `dl` are not reported).

    python3 scripts/check_test_links.py     # exit 1 on an archive named bare, or a file not read

Sibling of scripts/check_test_ports.py and scripts/check_test_databases.py.
"""
import os
import re
import sys

ROOTS = ['tests/c', 'performance/c', 'stress/c', 'yunos/c', 'utils/c', 'modules/c']
SKIP_DIRS = {'build', '.git'}
BARE_ARCHIVE = re.compile(r'lib[A-Za-z0-9_+.-]+\.a')
DASH_L = re.compile(r'-l([A-Za-z0-9_+.-]+)')
TLL = re.compile(r'target_link_libraries\s*\(([^)]*)\)', re.S)
KEYWORDS = {'PUBLIC', 'PRIVATE', 'INTERFACE', 'debug', 'optimized', 'general'}


def strip_comments(text):
    out = []
    for line in text.split('\n'):
        i = line.find('#')
        out.append(line if i < 0 else line[:i])
    return '\n'.join(out)


def tokens(text):
    for m in re.finditer(r'"[^"]*"|[^\s()"]+', text):
        yield m.start(), m.group(0).strip('"')


def sdk_archives(base, problems):
    """{foo: the CMake variable of its directory} for every libfoo.a of outputs/lib and outputs_ext/lib."""
    names = {}
    for d, var in (('outputs/lib', '${LIB_DEST_DIR}'), ('outputs_ext/lib', '${EXT_LIB_DIR}')):
        p = os.path.join(base, d)
        try:
            entries = os.listdir(p)
        except OSError as e:
            problems.append(f'cannot list {d}: {e.strerror}: bare target names and -l are not checked')
            continue
        for f in entries:
            if f.startswith('lib') and f.endswith('.a'):
                names.setdefault(f[3:-2], var)
    return names


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    problems = []
    archives = sdk_archives(base, problems)
    found = 0
    checked = 0

    def walk_error(e):
        problems.append(f'cannot list {os.path.relpath(e.filename, base)}: {e.strerror}')

    for root in ROOTS:
        top = os.path.join(base, root)
        if not os.path.isdir(top):
            problems.append(f'{root} not found')
            continue
        for d, dirs, files in os.walk(top, onerror=walk_error):
            dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
            if 'CMakeLists.txt' not in files:
                continue
            p = os.path.join(d, 'CMakeLists.txt')
            rel = os.path.relpath(p, base)
            try:
                with open(p, errors='replace') as fh:
                    text = strip_comments(fh.read())
            except OSError as e:
                # A file not read is a link not checked: said, and a failure
                problems.append(f'cannot read {rel}: {e.strerror}')
                continue
            checked += 1

            reports = []

            def report(pos, what, fix):
                line = text.count('\n', 0, pos) + 1
                reports.append((line, f'{rel}:{line}: {what}: write {fix}'))

            for pos, tok in tokens(text):
                if BARE_ARCHIVE.fullmatch(tok):
                    var = archives.get(tok[3:-2], '${LIB_DEST_DIR}')
                    report(pos, f'{tok} is named bare', f'{var}/{tok} (or a list of project.cmake)')
                    continue
                m = DASH_L.fullmatch(tok)
                if m and m.group(1) in archives:
                    report(pos, f'{tok} is a -l search of an SDK archive',
                           f'{archives[m.group(1)]}/lib{m.group(1)}.a')
            for block in TLL.finditer(text):
                args = list(tokens(block.group(1)))[1:]     # the first one is the target
                for pos, tok in args:
                    if tok in KEYWORDS or tok.startswith('$') or '/' in tok:
                        continue
                    if tok in archives:
                        report(block.start(1) + pos, f'{tok} is a bare name of an SDK archive',
                               f'{archives[tok]}/lib{tok}.a')
            for line, msg in sorted(reports):
                print(msg)
            found += len(reports)

    for p in problems:
        print(p, file=sys.stderr)
    if found:
        print(f'{found} archive(s) named bare in {checked} CMakeLists.txt: the binaries that '
              'link them are not relinked when they change')
    elif not problems:
        print(f'{checked} CMakeLists.txt checked: no archive named bare')
    if problems:
        print(f'{len(problems)} place(s) could not be read: their links are not checked')
    if found or problems:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
