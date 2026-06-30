# modules/c/console

Optional console / terminal GClasses, enabled with `CONFIG_MODULE_CONSOLE=y`. Provides interactive terminal helpers and TTY handling used by CLI-style tools (e.g. `ycli`, `ycommand`, `controlcenter`).

Depends on `ncurses`. Enable the module in `menuconfig` and rebuild with `yunetas build`.

## Source layout

The compiled library is built **only** from the files listed in `CMakeLists.txt`:

- `src/c_editline.c` — the `C_EDITLINE` GClass: the actual line editor. It is an
  ncurses-based reimplementation of linenoise's editing primitives (cursor
  movement, history, reverse-i-search, completion popup), wrapped in a gobj/FSM.
  Rendering goes through ncurses windows (`waddnstr`/`wmove`/`wrefresh`), and
  colour through curses attributes — not raw terminal escapes.
- `src/help_ncurses.c`, `src/g_ev_console.c`, `src/g_st_console.c` — supporting
  console helpers and event/state tables.

## `linenoise.c` / `linenoise.h` — reference snapshot, NOT compiled

`linenoise.c` and `linenoise.h` in the module root are a **vendored reference
snapshot** of [antirez/linenoise](http://github.com/antirez/linenoise) (upstream
`main`). They are kept **only as a reference** to diff against when deciding
whether an upstream fix needs to be ported into `src/c_editline.c`.

They are deliberately **not** part of the build:

- not listed in `CMakeLists.txt` `SRCS` (so never compiled into the library),
- not `#include`d by any source in the repo,
- not referenced by any `CMakeLists.txt` / `.cmake`.

Updating these files therefore has **no effect on the compiled console** — it
only refreshes the comparison base. When porting an upstream change, edit
`src/c_editline.c`; do not assume `linenoise.c` is live code. Note that most of
upstream's recent additions (paste folding, bracketed paste, dynamic buffer
growth) are built on linenoise's raw-terminal model, which `c_editline` replaced
with ncurses, so they generally do not apply here.
