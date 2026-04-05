# utils/c

Command-line utilities written in C that ship with Yuneta. They fall in three broad groups:

## Control-plane tools

| Tool | Purpose |
|---|---|
| `ycommand` | Send commands / stats requests to a running yuno |
| `ystats` | Subscribe to real-time statistics |
| `ybatch` | Run a batch script of commands |
| `ytests` | Run test suites against a live service |
| `ylist` | List running yunos on the local host |
| `yshutdown` | Shut down yunos / the agent |

## Data / storage inspection

| Tool | Purpose |
|---|---|
| `tr2list` | List records in a timeranger2 topic |
| `tr2keys` | List keys in a timeranger2 topic |
| `tr2search` | Search records by filter |
| `tr2migrate` | Migrate from legacy timeranger (v1) to timeranger2 |
| `treedb_list` | List nodes of a TreeDB graph db |
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
| `yscapec` | Escape a file's contents into a C string literal |
| `ytestconfig` | Validate a JSON configuration file |
| `json_diff` | Semantic diff of two JSON files |
| `time2date` / `time2range` | Timestamp conversion helpers |
| `pkey_to_jwks` | Convert RSA private key to JWKS |

Each tool has its own `README.md` with CLI usage details. All are built by `yunetas build` and installed into `$YUNETAS_OUTPUTS/bin/`.
