(util-yclone-gclass)=
# `yclone-gclass`

Clone a GClass under a new name, rewriting filenames and the
lower / Capitalized / UPPER identifier variants.

## Usage

```bash
yclone-gclass <OLD_GCLASS> <NEW_GCLASS>
```

```bash
yclone-gclass postgres authenticate     # c_postgres.{c,h} -> c_authenticate.{c,h}
```

Give the names **without** the `c_` prefix or `.c`/`.h` extension. Both names
are required. No options beyond argp's built-in `--help` / `--version`. The
files are read from, and written to, the current directory; a destination
that exists is refused, and a failed `.c` takes the new `.h` away with it.

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

```text
$ yclone-gclass time clock
Creating filename: c_clock.h
Creating filename: c_clock.c

Left as they were (a key inside a longer word), review them by hand:
    timeout                                  5
    Timeout                                  2
    timer                                    4
    C_TIMER                                  1
    set_timeout                              1
    clear_timeout                            1
    ac_timeout                               2
    EV_TIMEOUT                               2

Remember to change file headers (copyright, year, description)
```

That is the `gclass_service` template cloned: every word in the list belongs
to the timer it carries, and stays.

## See also

- [`yclone-project`](yclone-project.md) — clone a whole project.
- [`utils/c/yclone-gclass/README.md`](https://github.com/artgins/yunetas/blob/7.28.9/utils/c/yclone-gclass/README.md).
