(util-yclone-project)=
# `yclone-project`

Clone an entire project under a new name, replacing a source keyword with a
destination keyword in both filenames and file contents.

## Usage

```bash
yclone-project <SOURCE-PROJECT> <DESTINATION-PROJECT> --src-keyword <kw> --dst-keyword <kw>
```

| Argument / Option | Purpose |
|-------------------|---------|
| `<SOURCE-PROJECT>` | Existing project directory (positional, required) |
| `<DESTINATION-PROJECT>` | New directory — must not exist (positional, required) |
| `--src-keyword` / `-s` | Keyword to replace (required) |
| `--dst-keyword` / `-d` | Replacement keyword (required) |

Both positionals **and** both keyword options are mandatory. The destination
must not exist and must not be inside the source.

```bash
yclone-project ./gate_frigo ./gate_horno -s frigo -d horno
```

## What is replaced

The keyword is replaced in its lower, Capitalized and UPPER forms, only as a
**whole word**: the characters on each side must not be letters or digits.
`_`, `-`, `.` and `/` are separators. One pass, the longest key first, so a
new name that holds the old one is not replaced twice.

```text
keyword time -> clock
c_time.c            -> c_clock.c          (replaced)
register_c_time()   -> register_c_clock() (replaced)
C_TIME              -> C_CLOCK            (replaced)
C_TIMER, timeout    -> left as they were  (time inside a longer word)
```

At the end the tool lists every word that held the keyword and was left as
it was, with its count: review them by hand (a plural, `times`, is one of
them).

## What is copied

| Found | Done |
|-------|------|
| Text file | name and content rendered, mode kept |
| Binary file (holds a NUL byte) | copied byte for byte, name rendered, mode kept |
| Symlink | name rendered; a target inside the tree follows its new name (`run.link -> run_old.sh` becomes `run.link -> run_new.sh`); a target that is another link follows THAT link's name, which never gets a suffix (`run2.link -> run.link` stays so in a template); an absolute target, one outside the tree, or a dangling one is kept as it is |
| `.git`, `.hg`, `.svn`, `build`, `cmake-build-*`, `node_modules`, `dist` | not cloned (VCS and build output) |

The tool stops at the first error, says so, and exits non-zero; the
destination is left as far as it got. A destination path longer than
`PATH_MAX` is such an error (*"path too long"*).

## See also

- [`yclone-gclass`](yclone-gclass.md) — clone a single GClass.
- [`utils/c/yclone-project/README.md`](https://github.com/artgins/yunetas/blob/7.29.2/utils/c/yclone-project/README.md).
