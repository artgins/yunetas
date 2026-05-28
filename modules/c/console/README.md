# modules/c/console

Optional console / terminal GClasses, enabled with `CONFIG_MODULE_CONSOLE=y`. Provides interactive terminal helpers and TTY handling used by CLI-style tools (e.g. `ycli`, `ycommand`, `controlcenter`).

Depends on `ncurses` and `linenoise` from `kernel/c/linux-ext-libs/`.

Enable the module in `menuconfig` and rebuild with `yunetas build`.
