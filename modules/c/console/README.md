# modules/c/console

This optional module has the console and terminal GClasses. `CONFIG_MODULE_CONSOLE=y` enables it. It gives interactive terminal helpers and TTY handling to CLI tools, for example `ycli`, `ycommand` and `controlcenter`.

The module needs `ncurses`. To use it, enable the module in `menuconfig`. Then build again with `yunetas build`.

## Source layout

The build compiles the library only from the files that `CMakeLists.txt` lists:

- `src/c_editline.c`: the `C_EDITLINE` GClass, which is the line editor. It
  implements again the editing primitives of linenoise with ncurses (cursor
  movement, history, reverse-i-search, completion popup), inside a gobj and its FSM.
  It draws through ncurses windows (`waddnstr`/`wmove`/`wrefresh`), and it sets the
  color through curses attributes. It does not use raw terminal escapes.
- `src/help_ncurses.c`, `src/g_ev_console.c`, `src/g_st_console.c`: console
  helpers and the tables of events and states.

## `linenoise.c` / `linenoise.h` — reference snapshot, NOT compiled

`linenoise.c` and `linenoise.h` in the module root are a vendored reference
snapshot of [antirez/linenoise](http://github.com/antirez/linenoise) (upstream
`main`). A vendored snapshot is a copy of external code kept in this repo. The files are only a reference. You compare them with
upstream to decide if an upstream fix must go into `src/c_editline.c`.

They are not part of the build, on purpose:

- `CMakeLists.txt` does not list them in `SRCS`, so the library never contains them.
- No source file in the repo includes them with `#include`.
- No `CMakeLists.txt` or `.cmake` file refers to them.

As a result, a change to these files has no effect on the compiled console. It
only changes the base for the comparison. To port an upstream change, edit
`src/c_editline.c`. Do not think that `linenoise.c` is live code. Most of the
recent additions of upstream (paste folding, bracketed paste, dynamic buffer
growth) use the raw-terminal model of linenoise. `c_editline` replaced that model
with ncurses, so these additions usually do not apply here.
