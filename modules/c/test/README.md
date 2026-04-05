# modules/c/test

Support GClasses used exclusively by the test suites in `tests/c/`. Enabled with `CONFIG_MODULE_TEST=y`.

Provides helper gobjs (fake servers, counters, event dumpers, …) that would otherwise clutter real modules. Nothing here should be linked into production yunos.
