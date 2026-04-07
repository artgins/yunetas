# Utilities

Command-line tools shipped with Yuneta. All binaries are installed into
`$YUNETAS_OUTPUTS/bin/` by `yunetas build`.

**Source:** `utils/c/`

## Control Plane

| Binary | Description |
|--------|-------------|
| **`ycommand`** | Control-plane CLI — sends commands and stats requests to a running yuno over a local socket or WebSocket. Supports single-command and interactive shell modes. |
| **`ybatch`** | Batch command runner — executes a script of commands against a yuno. Useful for automation, provisioning, and smoke tests. |
| **`ystats`** | Real-time statistics subscriber — displays live stats from a yuno with configurable refresh interval and filtering. |
| **`ytests`** | Test-suite runner against a live service — supports repeat/interval options for soak testing. |
| **`ylist`** | List running yunos on the local machine with process information. Use `--pids` for scripting. |
| **`yshutdown`** | Shut down all Yuneta processes on the local machine. Supports selective modes (`--agent-only`, `--keep-agent`). |

## Data Inspection

| Binary | Description |
|--------|-------------|
| **`tr2list`** | List records from a timeranger2 topic. Supports time-range filtering, field selection, metadata display, and local timestamps. |
| **`tr2keys`** | List all keys (primary ids) stored in a timeranger2 topic. |
| **`tr2search`** | Search records inside a timeranger2 topic by content filters and key, with optional base64 decoding. |
| **`tr2migrate`** | Migrate data from legacy timeranger (v1) to timeranger2. |
| **`treedb_list`** | List nodes of a TreeDB graph database — walks topics and their hook/fkey relationships. |
| **`msg2db_list`** | List messages from a msg2db database (dict-style message store on timeranger2). |
| **`list_queue_msgs2`** | List messages from a msg2db persistent queue. |
| **`stats_list`** | List persisted statistics with filtering by group, variable, metric, units, and time range. |
| **`json_diff`** | Semantic diff of two JSON files — supports unordered array comparison and metadata/private-key filtering. |

## Code Generation

| Binary | Description |
|--------|-------------|
| **`yuno-skeleton`** | Create a new yuno from a pre-defined template. Use `--list` to see available skeletons. |
| **`ymake-skeleton`** | Generate a yuno skeleton from an existing project, renaming identifiers to match a new role. |
| **`yclone-gclass`** | Clone a GClass template under a new name — rewrites file names and identifiers automatically. |
| **`yclone-project`** | Clone a whole Yuneta project (yuno, util, …) under a new name. |
| **`yscapec`** | Escape a file's contents into a C string literal. Handy for embedding JSON, SQL, or HTML in `.c` source. |

## Diagnostics

| Binary | Description |
|--------|-------------|
| **`fs_watcher`** | Test harness for the `fs_watcher` library — prints every filesystem event on a path. |
| **`inotify`** | Raw Linux `inotify` diagnostic tool — decodes and prints inotify event flags. |
| **`ytestconfig`** | Validate a JSON configuration file — exits non-zero on parse failure. |
| **`time2date`** | Bidirectional converter between Unix timestamps and ASCII date/time strings. |
| **`time2range`** | Convert a Unix timestamp into a human-friendly time range (seconds → years). |
| **`static_binary_test`** | Smoke test for fully-static builds (`CONFIG_FULLY_STATIC`). |

## Security

| Binary | Description |
|--------|-------------|
| **`keycloak_pkey_to_jwks`** | Convert an RSA private key (base64 DER) to a JWKS document for OAuth 2 / JWT providers. |
