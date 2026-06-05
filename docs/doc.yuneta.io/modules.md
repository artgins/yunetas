# Built-in Modules

Optional modules that extend the kernel with additional GClasses.
Each module is controlled by a `CONFIG_MODULE_*` option in `.config`
(enable/disable via `menuconfig`). All are enabled by default.

**Source:** `modules/c/`

Each module has its own page (see the navigation sidebar):

| Module | Kconfig | GClasses | Description |
|--------|---------|----------|-------------|
| [**Console**](modules/console.md) | `CONFIG_MODULE_CONSOLE` | `C_EDITLINE` | Interactive terminal line editing with history (based on [linenoise](https://github.com/antirez/linenoise)). Used by [`ycli`](#util-ycli), [`ycommand`](#util-ycommand) and [`mqtt_tui`](#util-mqtt_tui). |
| [**Modbus**](modules/modbus.md) | `CONFIG_MODULE_MODBUS` | `C_PROT_MODBUS_M` | [Modbus](https://en.wikipedia.org/wiki/Modbus) protocol master — reads/writes device registers (coils, discrete inputs, input/holding registers). For industrial IoT. |
| [**MQTT**](modules/mqtt.md) | `CONFIG_MODULE_MQTT` | `C_PROT_MQTT`, `C_PROT_MQTT2`, `C_MQTT_BROKER` | Full MQTT protocol implementation (v3.1.1 + v5.0) with a persistent message broker backed by TreeDB. |
| [**Postgres**](modules/postgres.md) | `CONFIG_MODULE_POSTGRES` | `C_POSTGRES` | [PostgreSQL](https://www.postgresql.org/) integration — async query execution with automatic JSON-to-SQL type mapping. Requires `libpq`. |
| [**Test**](modules/test.md) | `CONFIG_MODULE_TEST` | `C_PEPON`, `C_TESTON` | Paired test server/client for functional testing and traffic generation. |
