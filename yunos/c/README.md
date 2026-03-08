# yunos

Deployable Yuneta services. Each yuno is a standalone executable built around one or more GClasses, with a JSON configuration that defines its runtime topology (services, I/O gates, protocols, persistence). All yunos use the Yuneta entry point (`yuneta_entry_point`) and event loop.

## Directory Layout

```
yunos/
└── c/
    ├── yuno_agent/         # Realm agent — primary authority on a host
    ├── yuno_agent22/       # Secondary agent — remote entry for controlcenter
    ├── controlcenter/      # Central management of distributed agents
    ├── logcenter/          # Centralized log collection, analysis, and alerting
    ├── mqtt_broker/        # MQTT v3.1/v3.1.1/v5.0 broker with persistence
    ├── mqtt_tui/           # Interactive MQTT client (TUI)
    ├── emailsender/        # Email sending service via libcurl/SMTP
    ├── dba_postgres/       # PostgreSQL database adapter
    ├── sgateway/           # Simple gateway for protocol bridging
    ├── emu_device/         # Device emulator for testing
    ├── watchfs/            # Filesystem watcher — run commands on file changes
    └── yuno_cli/           # Interactive CLI with ncurses windowing (ycli)
```

## Yuno Summary

| Yuno | Role | GClass | Install path | Description |
|------|------|--------|-------------|-------------|
| yuneta_agent | `yuneta_agent` | `C_AGENT` | `/yuneta/agent` | Primary host authority — realm, yuno, and binary lifecycle management |
| yuneta_agent22 | `yuneta_agent22` | `C_AGENT22` | `/yuneta/agent` | Secondary agent — connects to controlcenter |
| yuneta_agent44 | `yuneta_agent44` | `C_AGENT22` | `/yuneta/agent` | Third agent variant (reuses `C_AGENT22`) |
| controlcenter | `controlcenter` | `C_CONTROLCENTER` | `${YUNOS_DEST_DIR}` | Central management of distributed agents |
| logcenter | `logcenter` | `C_LOGCENTER` | `${YUNOS_DEST_DIR}` | Log collection with email alerts |
| mqtt_broker | `mqtt_broker` | `C_MQTT_BROKER` | `${YUNOS_DEST_DIR}` | MQTT broker (v3.1, v3.1.1, v5.0) |
| mqtt_tui | `mqtt_tui` | `C_MQTT_TUI` | `/yuneta/bin` | Interactive MQTT client |
| emailsender | `emailsender` | `C_EMAILSENDER`, `C_CURL` | `${YUNOS_DEST_DIR}` | Email sending via SMTP/libcurl |
| dba_postgres | `dba_postgres` | `C_DBA_POSTGRES` | `${YUNOS_DEST_DIR}` | PostgreSQL adapter |
| sgateway | `sgateway` | `C_SGATEWAY` | `${YUNOS_DEST_DIR}` | Simple gateway |
| emu_device | `emu_device` | `C_EMU_DEVICE` | `${YUNOS_DEST_DIR}` | Device gate emulator |
| watchfs | `watchfs` | `C_WATCHFS` | `${YUNOS_DEST_DIR}`, `/yuneta/bin` | Filesystem watcher |
| ycli | `ycli` | `C_CLI` + 7 window GClasses | `/yuneta/bin` | Ncurses CLI |

---

## Core Infrastructure Yunos

### yuneta_agent

The **primary authority** on a host. Manages realms, yunos, binaries, configurations, and public services. Listens on two WebSocket ports for control-plane commands.

**Ports (default):**
- `ws://0.0.0.0:1991` — Primary control port
- `wss://0.0.0.0:1993` — TLS control port (with certificate)

**Memory:** 200 MB max block, 2 GB max system, 200 MB superblock

**TreeDB schema:** `treedb_schema_yuneta_agent.c` — persistent storage for realms, yunos, binaries, configs, public services

**Commands (50+):**

Agent management:
| Command | Description |
|---------|-------------|
| `help` | Command help |
| `authzs` | Authorization help |
| `list-consoles` | List connected consoles |
| `open-console` | Open a console |
| `close-console` | Close a console |
| `command-agent` | Send command to agent |
| `stats-agent` | Get agent statistics |
| `set-ordered-kill` | Kill yunos with SIGQUIT (ordered) |
| `set-quick-kill` | Kill yunos with SIGKILL (quick) |

Filesystem operations:
| Command | Description |
|---------|-------------|
| `dir-logs` | List log files of a yuno |
| `dir-local-data` | List local data files |
| `dir-yuneta` | List `/yuneta` directory |
| `dir-realms` | List `/yuneta/realms` directory |
| `dir-repos` | List `/yuneta/repos` directory |
| `dir-store` | List `/yuneta/store` directory |
| `write-tty` | Write data to tty |
| `read-json` | Read JSON file |
| `read-file` | Read text file |
| `read-binary-file` | Read binary file (base64) |
| `running-keys` | Read yuno running parameters |
| `running-bin` | Read yuno binary path |
| `check-json` | Check JSON refcounts |

Deployment:
| Command | Description |
|---------|-------------|
| `replicate-node` | Replicate realms to another node |
| `upgrade-node` | Upgrade realms on another node |
| `replicate-binaries` | Replicate binaries to another node |
| `delete-public-service` | Remove a public service |

Realm management:
| Command | Description |
|---------|-------------|
| `check-realm` | Check if a realm exists |
| `create-realm` | Create a new realm |
| `update-realm` | Update a realm |
| `delete-realm` | Remove a realm |

Binary management:
| Command | Description |
|---------|-------------|
| `install-binary` | Install yuno binary |
| `update-binary` | Update binary (dev only) |
| `delete-binary` | Delete binary |

Configuration:
| Command | Description |
|---------|-------------|
| `create-config` | Create configuration |
| `edit-config` | Edit configuration |
| `view-config` | View configuration |
| `update-config` | Update configuration |
| `delete-config` | Delete configuration |

Yuno lifecycle:
| Command | Description |
|---------|-------------|
| `find-new-yunos` | Find new yunos |
| `create-yuno` | Create yuno |
| `delete-yuno` | Delete yuno |
| `set-tag` | Set yuno tag |
| `set-multiple` | Set yuno multiple instances |
| `edit-yuno-config` | Edit yuno configuration |
| `view-yuno-config` | View yuno configuration |
| `run-yuno` | Run yuno |
| `kill-yuno` | Kill yuno |
| `play-yuno` | Play (resume) yuno |
| `pause-yuno` | Pause yuno |
| `enable-yuno` | Enable yuno |
| `disable-yuno` | Disable yuno |
| `trace-on-yuno` | Enable tracing |
| `trace-off-yuno` | Disable tracing |
| `command-yuno` | Send command to yuno |
| `stats-yuno` | Get yuno statistics |
| `authzs-yuno` | Get yuno permissions |

Listing and inspection:
| Command | Description |
|---------|-------------|
| `top` | List enabled yunos |
| `list-yunos` | List all yunos |
| `list-binaries` | List binaries |
| `list-configs` | List configurations |
| `list-realms` | List realms |
| `list-public-services` | List public services |
| `list-yunos-instances` | List yuno instances |
| `list-binaries-instances` | List binary instances |
| `list-configs-instances` | List config instances |
| `list-realms-instances` | List realm instances |
| `list-public-services-instances` | List public service instances |

Snapshots:
| Command | Description |
|---------|-------------|
| `snaps` | List snapshots |
| `snap-content` | Show snapshot content |
| `shoot-snap` | Create snapshot |
| `activate-snap` | Activate snapshot |
| `deactivate-snap` | Deactivate snapshot |

Other:
| Command | Description |
|---------|-------------|
| `ping` | Ping |
| `node-uuid` | Get UUID of node |

**Extra files:**
- `certs/` — Self-signed TLS certificates (`yuneta_agent.crt`, `yuneta_agent.key`, `localhost.crt`, `localhost.key`)
- `create-certs-self-signed/` — Scripts to generate certificates
- `service/` — SysV init script (`install-yuneta-service.sh`, `remove-yuneta-service.sh`, `yuneta_agent`)

### yuneta_agent22

A **secondary agent** providing a remote entry point for the `controlcenter`. Connects outbound to a controlcenter instance (default: port 1994). Operates with `master: false` authorization (cannot modify realm topology).

**Commands:** `help`, `write-tty`, `list-consoles`, `open-console`, `close-console`

### yuneta_agent44

A **third agent variant** (built from `main44.c`). Reuses the `C_AGENT22` GClass with its own identity. Built as a separate binary from the same CMakeLists.txt as agent22.

### controlcenter

**Central management** of distributed Yuneta agents. Agents connect in and the controlcenter dispatches commands across the fleet.

**TreeDB schema:** `treedb_schema_controlcenter.c`

**Commands:**

| Command | Description |
|---------|-------------|
| `help` | Command help |
| `authzs` | Authorization help |
| `logout-user` | Logout a user |
| `list-agents` | List connected agents |
| `command-agent` | Send command to agent (by UUID or hostname) |
| `stats-agent` | Get agent statistics |
| `drop-agent` | Drop agent connection |
| `write-tty` | Write data to tty |

---

## Service Yunos

### logcenter

Collects log messages from all yunos on the host via UDP (default `udp://127.0.0.1:1992`). Provides log analysis, search, email summaries, and disk/memory monitoring with automatic restart on queue alarms.

**Key Attributes:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `url` | string | `udp://127.0.0.1:1992` | UDP server URL |
| `from` | string | — | Email from field (persistent) |
| `to` | string | — | Email to field (persistent) |
| `subject` | string | `"Log Center Summary"` | Email subject |
| `log_filename` | string | `"W.log"` | Log filename mask (DD/MM/CCYY-W-ZZZ) |
| `max_rotatoryfile_size` | integer | — | Max log file size (MB) |
| `min_free_disk` | integer | — | Minimum free disk percentage |
| `min_free_mem` | integer | — | Minimum free memory percentage |
| `restart_on_alarm` | boolean | false | Restart yuneta on queue alarm |
| `restart_yuneta_command` | string | `yshutdown -s; ...` | Restart command |
| `timeout_restart_yuneta` | integer | 3600 | Min seconds between restarts |

**Commands:**

| Command | Description |
|---------|-------------|
| `help` | Command help |
| `display-summary` | Display summary report |
| `send-summary` | Send summary by email |
| `enable-send-summary` | Enable email summaries |
| `disable-send-summary` | Disable email summaries |
| `reset-counters` | Reset counters |
| `search` | Search in log messages |
| `tail` | Output last part of log messages |
| `restart-yuneta-on-queue-alarm` | Configure automatic restart |

### mqtt_broker

MQTT broker supporting v3.1, v3.1.1, and v5.0. Uses the `C_MQTT_BROKER` GClass from the mqtt module. Persistence via TimeRanger2 and TreeDB.

**Memory:** 1 GB max block, 16 GB max system, 1 GB superblock (sized for high-throughput message handling).

**Default deny list:** `test/nosubscribe`

**Test suite** (in `tests/`):
- `mqtt_connect_disconnect_test.py` — Connect/disconnect test with paho-mqtt
- `mqtt_benchmark.py` — Throughput benchmark
- `run_hivemq_tests.sh` — HiveMQ MQTT CLI conformance tests
- `mqtt_broker.json` — Sample broker configuration

### emailsender

Email sending service using libcurl/SMTP. Queues email requests with TimeRanger2 persistence and processes them asynchronously.

**GClasses:** `C_EMAILSENDER` (queue/dispatch), `C_CURL` (libcurl wrapper)

**Events:** `EV_CURL_COMMAND` (send request), `EV_CURL_RESPONSE` (receive result)

**Dependency:** `sudo apt-get install libcurl4-openssl-dev`

### dba_postgres

PostgreSQL database adapter. Wraps the `C_POSTGRES` module GClass with a yuno entry point.

**Conditional:** Only built when `CONFIG_C_POSTGRES` is enabled.

**Dependency:** `sudo apt-get install postgresql-server-dev-all libpq-dev`

### sgateway

Simple protocol gateway for bridging between different transport/protocol combinations. Configurable input/output topology.

### watchfs

Filesystem watcher that executes a shell command when monitored files change. Useful for auto-building documentation, triggering deployments, or development workflows.

**Key Attributes:**

| Name | Type | Description |
|------|------|-------------|
| `path` | string | Path to watch |
| `recursive` | boolean | Watch subdirectory tree |
| `patterns` | string | File patterns to match (e.g., `*.py;*.rst;*.json`) |
| `command` | string | Command to execute on change |
| `use_parameter` | boolean | Pass filename as parameter |
| `ignore_changed_event` | boolean | Ignore file modification events |
| `ignore_renamed_event` | boolean | Ignore file rename events |
| `info` | boolean | Report found subdirectories |

**Default config:** Watches `.` for `*.py`, `*.mako`, `*.js`, `*.css`, `*.scss`, `*.rst`, `*.json` files recursively, runs `make html`.

**Batch configs** (in `batches/yuneta_docs/`): Pre-built configurations for deploying Yuneta documentation sections.

---

## Client Yunos

### mqtt_tui

Interactive MQTT client with terminal UI (ncurses). Connects to an MQTT broker and a Yuneta broker simultaneously. Supports publish, subscribe, unsubscribe interactively.

**GClasses:** `C_MQTT_TUI`, `C_EDITLINE`

**Events:** `EV_SEND_MQTT_SUBSCRIBE`, `EV_SEND_MQTT_UNSUBSCRIBE`, `EV_SEND_MQTT_PUBLISH`

**Command-line options (30+):**

```
MQTT:
  -c, --mqtt-persistent-session      Enable persistent session
  -d, --mqtt-persistent-db           Persistent message database
  -i, --id CLIENT_ID                 MQTT Client ID
  -q, --mqtt_protocol PROTOCOL       mqttv5, mqttv311, or mqttv31
  -e, --mqtt_connect_properties JSON MQTT v5 CONNECT properties
  -x, --mqtt_session_expiry_interval SECONDS
  -a, --mqtt_keepalive SECONDS       Keep-alive (default: 60)
  --will-topic TOPIC                 Last Will topic
  --will-payload PAYLOAD             Last Will payload
  --will-qos QOS                     Last Will QoS
  --will-retain RETAIN               Last Will retain
  --will-properties JSON             Last Will properties (v5)

OAuth2:
  -K, --auth_system SYSTEM           OpenID system (default: keycloak)
  -k, --auth_url URL                 OpenID endpoint
  -Z, --azp AZP                      Authorized Party
  -u, --user_id USER_ID              Username or OAuth2 user ID
  -U, --user_passw PASSWORD          OAuth2 password
  -P, --mqtt_passw PASSWORD          MQTT password
  -j, --jwt JWT                      Pre-obtained JWT

Connection:
  -h, --url-mqtt URL                 MQTT URL (default: mqtt://127.0.0.1:1810)
  -b, --url-broker URL               Broker URL (default: ws://127.0.0.1:1800)

Local:
  -p, --print                        Print configuration
  -l, --verbose LEVEL                Verbose level
  -v, --version                      Print version
  -V, --yuneta-version               Print Yuneta version
  -m, --with-metadata                Print with metadata
  -7, --list-mqtt-properties         List MQTT v5 properties
```

### ycli (yuno_cli)

Full-featured interactive command-line interface with ncurses windowing. Registers 9 GClasses for its TUI:

| GClass | Purpose |
|--------|---------|
| `C_CLI` | Main CLI controller |
| `C_EDITLINE` | Line editor (from console module) |
| `C_WN_STDSCR` | Root ncurses screen |
| `C_WN_LAYOUT` | Window layout manager |
| `C_WN_BOX` | Box container |
| `C_WN_LIST` | Scrollable list |
| `C_WN_STATIC` | Static text display |
| `C_WN_STSLINE` | Status line |
| `C_WN_TOOLBAR` | Toolbar |

### emu_device

Device gate emulator for testing. Injects messages from a TimeRanger2 database into a remote service at configurable speed.

**Command-line options:**

```
Database:
  -u, --url URL              Target URL
  -a, --path PATH            Data path
  -b, --database DATABASE    Database name
  -c, --topic TOPIC          Topic name
  -d, --leading LEADING      Leading data (base64)

Search conditions:
  --from-t TIME              Start time
  --to-t TIME                End time
  --from-rowid ROWID         Start row ID
  --to-rowid ROWID           End row ID
  --user-flag-set MASK       User flag mask (set)
  --user-flag-not-set MASK   User flag mask (not set)
  --system-flag-set MASK     System flag mask (set)
  --system-flag-not-set MASK System flag mask (not set)

Injection speed:
  -w, --window NUMBER        Messages per interval (default: 1)
  -i, --interval MS          Interval in milliseconds (default: 1000)

Local:
  -p, --print                Print configuration
  -l, --verbose LEVEL        Verbose level
  -v, --version              Print version
  -V, --yuneta-version       Print Yuneta version
```

---

## Build Details

All yunos link the standard kernel libraries. Additional per-yuno dependencies:

| Yuno | Extra libraries |
|------|----------------|
| mqtt_broker | `${MODULE_MQTT}` |
| mqtt_tui | `${MODULE_CONSOLE}`, `${MODULE_MQTT}`, `panel.a`, `ncurses.a` |
| ycli | `${MODULE_CONSOLE}`, `panel.a`, `ncurses.a` |
| emailsender | `${CURL_LIBRARIES}` |
| dba_postgres | `libyunetas-c_postgres.a`, `pq` |

All yunos are built with `add_yuno_executable()` and follow the standard memory configuration pattern:

```c
json_t *kw = json_pack("{s:I, s:I, s:I, s:i}",
    "MEM_MAX_BLOCK", (json_int_t)max_block,
    "MEM_MAX_SYSTEM_MEMORY", (json_int_t)max_system,
    "MEM_SUPERBLOCK", (json_int_t)superblock,    // optional
    "MEM_MIN_BLOCK", 512
);
```

## Configuration

Yunos are configured via JSON files passed with `--config-file`. The JSON structure follows a standard pattern:

```json
{
    "global": {
        "Agent.agent_environment": {
            "daemon_log_handlers": {
                "to_file": { "filename_mask": "W.log" }
            }
        }
    },
    "environment": {
        "node_owner": "artgins"
    },
    "__json_config_variables__": {
        "__input_url__": "ws://0.0.0.0:1991"
    }
}
```

Configuration variables use `__double_underscore__` delimiters and are substituted at startup.
