# yunos

This directory holds the deployable Yuneta services. Each yuno is a standalone executable built around one or more GClasses. Its JSON configuration defines its runtime topology (services, I/O gates, protocols, persistence). All yunos use the Yuneta entry point (`yuneta_entry_point`) and the event loop.

## Directory Layout

```
yunos/
└── c/
    ├── yuno_agent/         # Realm agent — primary authority on a host
    ├── yuno_agent22/       # Secondary agent — remote entry for controlcenter
    ├── controlcenter/      # Central management of distributed agents
    ├── logcenter/          # Centralized log collection, analysis, and alerting
    ├── mqtt_broker/        # MQTT v3.1/v3.1.1/v5.0 broker with persistence
    ├── emailsender/        # Email sending service (native SMTPS)
    ├── webstats/          # Daily report of the node's web server logs
    ├── archive_sender/     # Ships the batches of a timeranger2 archive to a backup
    ├── archive_keeper/     # Keeps them on the backup machine
    ├── dba_postgres/       # PostgreSQL database adapter
    ├── sgateway/           # Simple gateway: raw TCP relay
    └── watchfs/            # Filesystem watcher — run commands on file changes
```

> The interactive clients `ycli` and `mqtt_tui`, and the `emu_device` emulator,
> were here before. They moved to [`utils/c/`](../../utils/c/)
> (`utils/c/ycli`, `utils/c/mqtt_tui`, `utils/c/emu_device`), because they are CLI
> tools installed in `/yuneta/bin`, not deployable services.

## Yuno Summary

| Yuno | Role | GClass | Install path | Description |
|------|------|--------|-------------|-------------|
| yuneta_agent | `yuneta_agent` | `C_AGENT` | `/yuneta/agent` | Primary host authority: realm, yuno, and binary lifecycle management |
| yuneta_agent22 | `yuneta_agent22` | `C_AGENT22` | `/yuneta/agent` | Secondary agent, connects to controlcenter |
| yuneta_agent44 | `yuneta_agent44` | `C_AGENT22` | `/yuneta/agent` | Third agent variant (reuses `C_AGENT22`) |
| controlcenter | `controlcenter` | `C_CONTROLCENTER` | `${YUNOS_DEST_DIR}` | Central management of distributed agents |
| logcenter | `logcenter` | `C_LOGCENTER` | `${YUNOS_DEST_DIR}` | Log collection with email alerts |
| mqtt_broker | `mqtt_broker` | `C_MQTT_BROKER` | `${YUNOS_DEST_DIR}` | MQTT broker (v3.1, v3.1.1, v5.0) |
| emailsender | `emailsender` | `C_EMAILSENDER`, `C_SMTP_SESSION` | `${YUNOS_DEST_DIR}` | Email sending via native SMTPS |
| webstats | `webstats` | `C_WEBSTATS`, `C_LOG_READER` | `${YUNOS_DEST_DIR}` | Daily report of the web server logs, by mail |
| archive_sender | `archive_sender` | `C_ARCHIVE_SENDER` | `${YUNOS_DEST_DIR}` | Ships the batches of a timeranger2 archive to a backup (native or rsync) |
| archive_keeper | `archive_keeper` | `C_ARCHIVE_KEEPER` | `${YUNOS_DEST_DIR}` | Keeps the batches that archive_senders ship natively |
| dba_postgres | `dba_postgres` | `C_DBA_POSTGRES` | `${YUNOS_DEST_DIR}` | PostgreSQL adapter |
| sgateway | `sgateway` | `C_SGATEWAY` | `${YUNOS_DEST_DIR}` | Simple gateway |
| watchfs | `watchfs` | `C_WATCHFS` | `${YUNOS_DEST_DIR}`, `/yuneta/bin` | Filesystem watcher |

---

## Core Infrastructure Yunos

### yuneta_agent

The primary authority on a host. It manages realms, yunos, binaries, configurations, and public services. It listens on two WebSocket ports for control-plane commands.

Ports (default):
- `ws://127.0.0.1:1991`: Primary control port
- `wss://0.0.0.0:1993`: TLS control port (with certificate)

Memory: 200 MB max block, 2 GB max system, 200 MB superblock

TreeDB schema: `treedb_schema_yuneta_agent.c`, the persistent storage for realms, yunos, binaries, configurations, public services

Commands (50+):

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
| `set-graceful-kill` | Kill yunos with SIGQUIT (graceful, the yuno stops itself) |
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
| `delete-public-service` | Remove a public service (`id=` required) |

Realm management:
| Command | Description |
|---------|-------------|
| `check-realm` | Check if a realm exists |
| `create-realm` | Create a new realm |
| `update-realm` | Update a realm |
| `delete-realm` | Remove a realm (`id=` required) |

Binary management:
| Command | Description |
|---------|-------------|
| `install-binary` | Install yuno binary |
| `update-binary` | Update binary (dev only) |
| `delete-binary` | Delete a binary (`id=` required. `version=`: one version. `record_only=1`: the record, its files stay) |

Configuration:
| Command | Description |
|---------|-------------|
| `create-config` | Create configuration |
| `edit-config` | Edit configuration |
| `view-config` | View configuration |
| `update-config` | Update configuration |
| `delete-config` | Delete a configuration (`id=` required. `version=`: one version) |

Yuno lifecycle:
| Command | Description |
|---------|-------------|
| `find-new-yunos` | Find new yunos |
| `create-yuno` | Create yuno |
| `delete-yuno` | Delete one release (`yuno_release=`) or the whole yuno (`whole=1`), not both. `id=` required |
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
| `trace-on-yuno` | Set global trace levels in the yuno (`level=machine,ev_kw`) |
| `trace-off-yuno` | Reset every global trace level of the yuno |
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

Extra files:
- `certs/`: Self-signed TLS certificates (`yuneta_agent.crt`, `yuneta_agent.key`, `localhost.crt`, `localhost.key`)
- `create-certs-self-signed/`: Scripts to generate certificates
- `service/`: the start and stop helpers of the `.deb` (`install-yuneta-service.sh`, `remove-yuneta-service.sh`) and the SysV script `yuneta_agent`. The helpers enable/disable the systemd units, or the SysV script when there is no systemd. All these files are identical, byte for byte, to the files of the packager.

### yuneta_agent22

A secondary agent that gives a remote entry point for the `controlcenter`. It connects outbound to a controlcenter instance (default: port 1989, `yunetacontrol.ovh:1989`). It operates with `master: false` authorization, so it cannot change the realm topology.

Commands: `help`, `write-tty`, `list-consoles`, `open-console`, `close-console`

### yuneta_agent44

A third agent variant (built from `main44.c`). It uses the `C_AGENT22` GClass again, with its own identity. It is a separate binary, built from the same CMakeLists.txt as agent22.

### controlcenter

Central management of distributed Yuneta agents. The agents connect in, and the controlcenter dispatches commands across the fleet. It also keeps the scenarios of the fleet and the runs of their actions. A scenario is a set of yunos that are watched and tested together.

TreeDB schema: `treedb_schema_controlcenter.c` (`scenarios`, `scenario_runs`)

Commands:

| Command | Description |
|---------|-------------|
| `help` | Command help |
| `authzs` | Authorization help |
| `logout-user` | Log out a user |
| `list-agents` | List connected agents |
| `command-agent` | Send command to agent (by UUID or hostname) |
| `stats-agent` | Get agent statistics |
| `drop-agent` | Drop agent connection |
| `write-tty` | Write data to tty |
| `scenarios` | List the scenarios, or one |
| `save-scenario` | Create or replace a scenario |
| `delete-scenario` | Delete a scenario and its runs |
| `run-scenario` | Run the steps of an action of a scenario, in order |
| `scenario-runs` | The runs of a scenario, newest first |

---

## Service Yunos

### logcenter

Collects log messages from all yunos on the host through UDP (default `udp://127.0.0.1:1992`). It gives log analysis, search, email summaries, and disk/memory monitoring, with automatic restart on queue alarms.

Key Attributes:

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `url` | string | `udp://127.0.0.1:1992` | UDP server URL |
| `from` | string | none | Email from field (persistent) |
| `to` | string | none | Email to field (persistent) |
| `subject` | string | `"Log Center Summary"` | Email subject |
| `log_filename` | string | `"W.log"` | Log filename mask (DD/MM/CCYY-W-ZZZ) |
| `max_rotatoryfile_size` | integer | none | Max log file size (MB) |
| `min_free_disk` | integer | none | Minimum free disk percentage |
| `min_free_mem` | integer | none | Minimum free memory percentage |
| `restart_on_alarm` | boolean | false | Restart yuneta on queue alarm |
| `restart_yuneta_command` | string | `yshutdown -s; ...` | Restart command, run with `system()`: configuration only |
| `timeout_restart_yuneta` | integer | 3600 | Min seconds between restarts |

Commands:

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

MQTT broker for v3.1, v3.1.1, and v5.0. It uses the `C_MQTT_BROKER` GClass from the mqtt module. TimeRanger2 and TreeDB give the persistence.

Memory: 1 GB max block, 16 GB max system, 1 GB superblock (sized for high-throughput message handling).

Default deny list: `test/nosubscribe`

Tests (in `tests/`, index in `tests/README.md`):
- `hivemq/`: HiveMQ MQTT CLI battery: spec compliance, functional and performance (25 tests). If `mqtt-cli.jar` is missing, `run_hivemq_tests.sh` downloads it.
- `connect_disconnect/`: raw-packet CONNECT / DISCONNECT edge cases (Python, 22 rows)
- `mosquitto/`: the broker test suite of Eclipse Mosquitto, through a stand-in (`setup.sh`, `run_all.sh`)
- `mqtt_broker.json`: broker configuration of the three

### emailsender

Email sending service that uses native SMTPS (implicit TLS, RFC 8314). It queues email requests with TimeRanger2 persistence and processes them asynchronously.

GClasses: `C_EMAILSENDER` (queue/dispatch + MIME encoder), `C_SMTP_SESSION` (SMTP protocol over `C_TCP`)

Events: `EV_SEND_EMAIL` / `EV_SEND_MESSAGE` (enqueue), `EV_ON_MESSAGE` (smtp result), `EV_SMTP_COMMAND` (internal dequeue)

Dependency: none beyond the standard yuneta toolchain.

### dba_postgres

PostgreSQL database adapter. It wraps the `C_POSTGRES` module GClass with a yuno entry point.

Conditional: Only built when `CONFIG_C_POSTGRES` is enabled.

Dependency: `sudo apt-get install postgresql-server-dev-all libpq-dev`

### sgateway

Raw TCP relay. It takes one connection on `input_url` and relays its bytes, both ways, to its own connection to `output_url`. It opens one output connection for each client, with back-pressure. See [`sgateway/README.md`](sgateway/README.md).

### watchfs

Filesystem watcher that executes a shell command when monitored files change. Use it to build documentation automatically, to start deployments, or in development workflows.

Key Attributes:

| Name | Type | Description |
|------|------|-------------|
| `path` | string | Path to watch |
| `recursive` | boolean | Watch subdirectory tree |
| `patterns` | string | File patterns to match (for example `*.py;*.rst;*.json`) |
| `command` | string | Command to execute on change |
| `use_parameter` | boolean | Pass filename as parameter |
| `ignore_changed_event` | boolean | Ignore file modification events |
| `ignore_renamed_event` | boolean | Ignore file rename events |
| `info` | boolean | Report found subdirectories |

Default configuration: Watches `.` recursively for `*.py`, `*.mako`, `*.js`, `*.css`, `*.scss`, `*.rst`, `*.json` files, and runs `make html`.

Batch configurations (in `batches/yuneta_docs/`): Pre-built configurations to deploy sections of the Yuneta documentation.

---

## Client / utility yunos (moved out)

> The interactive clients `ycli` and `mqtt_tui`, and the `emu_device` emulator,
> moved to `utils/c/`, because they are CLI tools installed in `/yuneta/bin`, not
> deployable services. See
> [`utils/c/ycli/README.md`](../../utils/c/ycli/README.md),
> [`utils/c/mqtt_tui/README.md`](../../utils/c/mqtt_tui/README.md) and
> [`utils/c/emu_device/README.md`](../../utils/c/emu_device/README.md).

---

## Build Details

All yunos link the standard kernel libraries. Some yunos have more dependencies:

| Yuno | Extra libraries |
|------|----------------|
| mqtt_broker | `${MODULE_MQTT}` |
| dba_postgres | `libyunetas-c_postgres.a`, `pq` |

All yunos are built with `add_yuno_executable()`. They use the standard pattern for the memory configuration:

```c
json_t *kw = json_pack("{s:I, s:I, s:I, s:i}",
    "MEM_MAX_BLOCK", (json_int_t)max_block,
    "MEM_MAX_SYSTEM_MEMORY", (json_int_t)max_system,
    "MEM_SUPERBLOCK", (json_int_t)superblock,    // optional
    "MEM_MIN_BLOCK", 512
);
```

## Configuration

You configure a yuno with the JSON files that you give with `--config-file`. The JSON structure uses a standard pattern:

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
    }
}
```

Configuration variables use `__double_underscore__` delimiters. The yuno
substitutes them at startup. Their place decides what they reach:

- A top-level `__json_config_variables__` has the scope of its own json text.
  In the `main.c` of a yuno, it is replaced when that text is loaded. In the
  json file, it reaches only that file.
- A `"global": {"<service>.__json_config_variables__": {...}}` is replaced when
  the service is created, after the json file is merged. Thus a node can
  override it.

The agent keeps its input urls (`ws://127.0.0.1:1991`, `wss://0.0.0.0:1993`)
at the top level of its `main.c`, so no json file can move them. It keeps its
`__output_url__` in `"global"`:

```json
"global": {
    "controlcenter.__json_config_variables__": {
        "__output_url__": "yunetacontrol.com:1988"
    }
}
```
