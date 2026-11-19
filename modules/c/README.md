# modules/c

The optional C modules. You enable each one in `menuconfig` with a `CONFIG_MODULE_*` option. Each subdirectory holds a group of related GClasses.

| Module | Kconfig option | Content |
|---|---|---|
| `console` | `CONFIG_MODULE_CONSOLE` | Console / terminal GClasses |
| `mbus` | `CONFIG_MODULE_MBUS` | M-Bus master (EN 13757) and its telegram decoder |
| `modbus` | `CONFIG_MODULE_MODBUS` | Modbus protocol GClasses |
| `mqtt` | `CONFIG_MODULE_MQTT` | MQTT v3.1.1 / v5.0 client and broker GClasses |
| `postgres` | `CONFIG_MODULE_POSTGRES` | PostgreSQL client GClasses (requires `libpq`) |
| `test` | `CONFIG_MODULE_TEST` | Support GClasses for the test suite |

To use a module, enable it in `.config`. Then run `yunetas build`.
