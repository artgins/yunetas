# Built-in Modules

Optional modules that extend the kernel with additional GClasses.
Each module is controlled by a `CONFIG_MODULE_*` option in `.config`
(enable/disable via `menuconfig`). All are enabled by default.

**Source:** `modules/c/`

| Module | Kconfig | GClasses | Description |
|--------|---------|----------|-------------|
| **Console** | `CONFIG_MODULE_CONSOLE` | `C_EDITLINE` | Interactive terminal line editing with history (based on linenoise). Used by `ycli`, `ycommand` and `mqtt_tui`. |
| **Modbus** | `CONFIG_MODULE_MODBUS` | `C_PROT_MODBUS_M` | Modbus protocol master — reads/writes device registers (coils, discrete inputs, input/holding registers). For industrial IoT. |
| **MQTT** | `CONFIG_MODULE_MQTT` | `C_PROT_MQTT`, `C_PROT_MQTT2`, `C_MQTT_BROKER` | Full MQTT protocol implementation (v3.1.1 + v5.0) with a persistent message broker backed by TreeDB. |
| **Postgres** | `CONFIG_MODULE_POSTGRES` | `C_POSTGRES` | PostgreSQL integration — async query execution with automatic JSON-to-SQL type mapping. Requires `libpq`. |
| **Test** | `CONFIG_MODULE_TEST` | `C_PEPON`, `C_TESTON` | Paired test server/client for functional testing and traffic generation. |

## Module GClasses detail

### C_EDITLINE (Console)

Line editor with keyboard input handling, cursor movement, text
manipulation, completion, and history navigation. Provides the input
layer for the `ycli`, `ycommand` and `mqtt_tui` clients.

**Events handled:** `EV_KEYCHAR`, `EV_EDITLINE_MOVE_START / _END / _LEFT / _RIGHT`,
`EV_EDITLINE_DEL_CHAR / _EOL / _LINE / _PREV_WORD`, `EV_EDITLINE_BACKSPACE`,
`EV_EDITLINE_COMPLETE_LINE`, `EV_EDITLINE_ENTER`,
`EV_EDITLINE_PREV_HIST / _NEXT_HIST`, `EV_EDITLINE_REVERSE_SEARCH / _FORWARD_SEARCH`
(incremental Ctrl+R / Ctrl+S history search), `EV_EDITLINE_SWAP_CHAR`,
`EV_SETTEXT` / `EV_GETTEXT`, `EV_CLEAR_HISTORY`.

**Public helpers (for clients):**

| Function | Purpose |
|----------|---------|
| `editline_set_completion_callback(gobj, cb, user_data)` | Register a TAB-completion callback; `cb` fills an `editline_completions_t` with candidates + optional descriptions via `editline_add_completion(lc, str, desc)`. |
| `editline_set_hints_callback(gobj, cb, free_cb, user_data)` | Register an inline-hint callback; `cb` returns a heap-allocated string (plus optional ANSI colour/bold) rendered to the right of the cursor in gray. `free_cb` releases the string after draw. |
| `editline_history_count(gobj)` / `editline_history_get(gobj, idx)` | Read-only access to the in-memory history (1-based) for bang expansion (`!N`), `!history`-style listings, or custom search UIs. |

History entries are de-duplicated on insert (bash `HISTCONTROL=erasedups` style)
and the file is overwritten from memory on `EV_EDITLINE_ENTER`.

### C_PROT_MODBUS_M (Modbus)

Modbus master protocol — supports four object types:

| Object | Access | Size |
|--------|--------|------|
| Coils | read-write | 1 bit |
| Discrete Inputs | read-only | 1 bit |
| Input Registers | read-only | 16 bits |
| Holding Registers | read-write | 16 bits |

Address range `0x0000`–`0xFFFF`. Configuration-driven slave device
mapping with data-format conversion and multiplier support.

### C_PROT_MQTT / C_PROT_MQTT2 (MQTT)

MQTT protocol with publish, subscribe, QoS levels, and event-driven
message handling. `C_PROT_MQTT2` is the current implementation;
`C_PROT_MQTT` is the legacy version kept for backward compatibility.

### C_MQTT_BROKER (MQTT)

Full MQTT message broker — subscriber management, message routing,
session handling, and persistent storage using TreeDB on timeranger2.
Used by the `mqtt_broker` yuno.

### C_POSTGRES (Postgres)

Async PostgreSQL client with an internal query queue. Maps JSON types
to PostgreSQL types:

| JSON type | PostgreSQL type |
|-----------|-----------------|
| string | `text` |
| integer | `bigint` |
| real | `double precision` |
| boolean | `boolean` |
| object | `text` (JSON serialized) |

Requires `libpq-dev` and `CONFIG_MODULE_POSTGRES=y`.

### C_PEPON / C_TESTON (Test)

Paired test components: `C_PEPON` is a test server that responds to
requests, `C_TESTON` is a test client that generates traffic
(via `EV_START_TRAFFIC`). Used for functional testing and validation
of Yuneta transport and protocol stacks.
