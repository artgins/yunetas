# utils/c

These are the command-line utilities, written in C, that ship with Yuneta. `yunetas build` builds all of them and installs them into `$YUNETAS_OUTPUTS/bin/`. Each tool has its own `README.md` with the details of its CLI usage.

The tables below put the tools in groups.

## Control-plane tools

| Tool | Purpose |
|---|---|
| `ycommand` | Send commands or stats requests to a running yuno |
| `ystats` | Subscribe to real-time statistics |
| `ybatch` | Run a batch script of commands |
| `ytests` | Run test suites against a live service |
| `ylist` | List running yunos on the local host |
| `yshutdown` | Stop yunos or the agent |

## Interactive clients

| Tool | Purpose |
|---|---|
| `ycli` | Ncurses windowed CLI for browsing yunos and sending commands |
| `mqtt_tui` | Interactive MQTT client with a text-based UI |

## Data / storage inspection

| Tool | Purpose |
|---|---|
| `tr2list` | List records in a timeranger2 topic |
| `tr2keys` | List keys in a timeranger2 topic |
| `tr2check` | Examine a topic that a load test filled: duplicates, gaps, checksums, rate, latency |
| `tr2search` | Search records by filter |
| `tr2migrate` | Migrate from legacy timeranger (v1) to timeranger2 |
| `treedb_list` | List nodes of a TreeDB graph database |
| `msg2db_list` | List messages in a msg2db topic |
| `list_queue_msgs2` | List messages in a msg2db queue |
| `stats_list` | List persisted statistics |

## Scaffolding / helpers

| Tool | Purpose |
|---|---|
| `yuno-skeleton` | Create a new yuno from a template |
| `ymake-skeleton` | Clone an existing project as a skeleton |
| `yclone-gclass` | Clone a GClass under a new name |
| `yclone-project` | Clone an entire project under a new name |
| `yscapec` | Escape the contents of a file into a C string literal |
| `ytestconfig` | Make sure that a JSON configuration file is valid |
| `json_diff` | Compare two JSON files by their content |
| `time2date` / `time2range` | Timestamp conversion helpers |
| `pkey_to_jwks` | Convert an RSA private key to JWKS |

## Testing / emulation

| Tool | Purpose |
|---|---|
| `emu_device` | Device-gate emulator. It sends recorded TimeRanger2 frames again to an ingest endpoint, at a controlled rate |
