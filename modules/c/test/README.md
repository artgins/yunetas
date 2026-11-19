# modules/c/test

This module holds support GClasses for the test suites in `tests/c/`. To enable it, set `CONFIG_MODULE_TEST=y`.

Do not link this module into production yunos.

The module gives helper gobjs, for example fake servers, counters, and event dumpers. These helpers are in this module so that they do not clutter the real modules.
