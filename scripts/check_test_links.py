#!/usr/bin/env python3
"""
Check that no test links an archive by its bare name.

A test links the installed archives of the SDK, and it is relinked when one
of them changes only if CMake knows the FILE: `${LIB_DEST_DIR}/libfoo.a`
(or a list of project.cmake, YUNETAS_KERNEL_LIBS and the like) is a
dependency of the link, while a bare `libfoo.a` in target_link_libraries()
becomes a `-l` search, and the test keeps running the old library after a
change of the one it tests. Up to 7.26.3, 18 tests named the kernel
archives bare; `make clean`, and then the reinstall of the root tree, hid it.

    python3 scripts/check_test_links.py     # exit 1 when a CMakeLists.txt names an archive bare

Sibling of scripts/check_test_ports.py and scripts/check_test_databases.py.
"""
import os
import re
import sys

ROOTS = ['tests/c', 'performance/c', 'stress/c']
BARE = re.compile(r'^\s*(lib[A-Za-z0-9_+-]+\.a)\b', re.M)


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    found = 0
    unreadable = 0
    for root in ROOTS:
        for d, dirs, files in os.walk(os.path.join(base, root)):
            dirs[:] = [x for x in dirs if x != 'build']
            if 'CMakeLists.txt' not in files:
                continue
            p = os.path.join(d, 'CMakeLists.txt')
            rel = os.path.relpath(p, base)
            try:
                with open(p, errors='replace') as fh:
                    text = fh.read()
            except OSError as e:
                # A file not read is a link not checked: said, and a failure
                print(f'cannot read {rel}: {e.strerror}', file=sys.stderr)
                unreadable += 1
                continue
            for m in BARE.finditer(text):
                line = text.count('\n', 0, m.start()) + 1
                print(f'{rel}:{line}: {m.group(1)} is named bare: '
                      f'write ${{LIB_DEST_DIR}}/{m.group(1)} (or a list of project.cmake)')
                found += 1

    if found:
        print(f'{found} archive(s) named bare: the tests that link them are not '
              'relinked when they change')
    if unreadable:
        print(f'{unreadable} file(s) could not be read: their links are not checked')
    if found or unreadable:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
