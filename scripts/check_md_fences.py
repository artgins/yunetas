#!/usr/bin/env python3
"""
Check that every fenced code block of the Markdown files closes.

CommonMark closes a fence only with a line of the same character (``` or
~~~), at least as long as the opening one, and NOTHING after it but spaces.
A paragraph glued to the closing fence (```` ``` A new test that ````) does
not close it: the block runs on, and the text below renders as code until
the next fence. myst builds it without a word. Reported here:

    - a line that would close the open block but has text after the fence;
    - a block still open at the end of the file.

    python3 scripts/check_md_fences.py            # the docs and the READMEs of the repo
    python3 scripts/check_md_fences.py <path>...  # these files or directories

Exit 1 on a finding, or a file not read. docs/doc.yuneta.io/deploy.sh runs it
on the site before building it.
"""
import os
import re
import sys

SKIP_DIRS = {'.git', 'build', '_build', 'node_modules', 'outputs', 'outputs_ext',
             'linux-ext-libs'}
# Submodules check their own Markdown
SKIP_PATHS = {'kernel/js', 'yunos/js', 'utils/python/tui_yunetas'}
FENCE = re.compile(r'^( {0,3})(`{3,}|~{3,})(.*)$')


def check_file(path, rel):
    findings = []
    with open(path, errors='replace') as fh:
        lines = fh.read().split('\n')
    open_fence = None       # (char, length, line number)
    for n, line in enumerate(lines, 1):
        m = FENCE.match(line)
        if not m:
            continue
        run, rest = m.group(2), m.group(3)
        if open_fence is None:
            if run[0] == '`' and '`' in rest:
                continue    # inline code, not a fence
            open_fence = (run[0], len(run), n)
            continue
        char, length, opened = open_fence
        if run[0] != char or len(run) < length:
            continue        # a shorter or other fence: content of the block
        if rest.strip() == '':
            open_fence = None
        else:
            findings.append(f'{rel}:{n}: the fence opened at line {opened} is followed by text '
                            f'here ("{line.strip()[:40]}"), so it does not close: '
                            'put the text on the next line')
    if open_fence is not None:
        findings.append(f'{rel}:{open_fence[2]}: a fenced block opened here never closes')
    return findings


def markdown_files(base, paths):
    for p in paths:
        p = os.path.join(base, p) if not os.path.isabs(p) else p
        if os.path.isfile(p):
            yield p
            continue
        for d, dirs, files in os.walk(p):
            rel_d = os.path.relpath(d, base)
            dirs[:] = [x for x in dirs if x not in SKIP_DIRS
                       and os.path.normpath(os.path.join(rel_d, x)) not in SKIP_PATHS]
            for f in files:
                if f.endswith('.md'):
                    yield os.path.join(d, f)


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    paths = sys.argv[1:] or ['.']
    findings = []
    unreadable = 0
    checked = 0
    for p in markdown_files(base, paths):
        rel = os.path.relpath(p, base)
        try:
            findings += check_file(p, rel)
            checked += 1
        except OSError as e:
            # A file not read is a file not checked: said, and a failure
            print(f'cannot read {rel}: {e.strerror}', file=sys.stderr)
            unreadable += 1
    for f in findings:
        print(f)
    if findings:
        print(f'{len(findings)} fenced block(s) that do not close, in {checked} Markdown file(s)')
    elif not unreadable:
        print(f'{checked} Markdown file(s) checked: every fenced block closes')
    if unreadable:
        print(f'{unreadable} file(s) could not be read: their fences are not checked')
    if findings or unreadable:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
