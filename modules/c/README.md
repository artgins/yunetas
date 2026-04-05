# modules/c

Optional C modules that can be enabled from `menuconfig` (`CONFIG_MODULE_*`). Each subdirectory bundles a group of related GClasses.

| Module | Kconfig option | Content |
|---|---|---|
| `console` | `CONFIG_MODULE_CONSOLE` | Console / terminal GClasses |
| `modbus` | — | Modbus protocol GClasses |
| `mqtt` | `CONFIG_MODULE_MQTT` | MQTT v3.1.1 / v5.0 client & broker GClasses |
| `postgres` | `CONFIG_MODULE_POSTGRES` | PostgreSQL client GClasses (requires `libpq`) |
| `test` | `CONFIG_MODULE_TEST` | Support GClasses for the test suite |

Enable the modules you need in `.config` and run `yunetas build`.
