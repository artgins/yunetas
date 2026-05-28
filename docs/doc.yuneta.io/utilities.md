# Utilities

Command-line tools shipped with Yuneta. All binaries are installed into
`$YUNETAS_OUTPUTS/bin/` by `yunetas build`.

**Source:** `utils/c/`

Each tool below has its own anchored entry, so it can be linked directly
(e.g. `[](#util-ycommand)`) and shows up individually in the page outline.

## Control Plane

(util-ycommand)=
### `ycommand`

Control-plane CLI — sends commands and stats requests to a running yuno over a
local socket or WebSocket. Single-command, interactive and long-lived
stdin-pipe modes. Interactive mode has TAB completion, inline parameter hints,
`Ctrl+R`/`Ctrl+S` history search, `!help` / `!history` / `!source` local
commands, bash-style `!!` / `!N` history expansion, did-you-mean suggestions on
errors and schema-driven table output. Non-interactive accepts commands either
via `-c`, as positional args or piped on stdin; the pipe mode is event-driven,
so the session stays alive between commands and a single OAuth2 round-trip
covers many dispatches. Supports `;` chaining and `-cmd` ignore-fail (the
`ybatch` convention). See
[utils/c/ycommand/README.md](https://github.com/artgins/yunetas/blob/main/utils/c/ycommand/README.md)
for the full syntax and shortcut reference.

(util-ybatch)=
### `ybatch`

Batch command runner — executes a JSON-formatted script of commands against a
yuno. Useful for automation, provisioning, and smoke tests. Complementary to
`ycommand`'s line-oriented scripting.

(util-ystats)=
### `ystats`

Real-time statistics subscriber — displays live stats from a yuno with
configurable refresh interval and filtering.

(util-ytests)=
### `ytests`

Test-suite runner against a live service — supports repeat/interval options for
soak testing.

(util-ylist)=
### `ylist`

List running yunos on the local machine with process information. Use `--pids`
for scripting.

(util-yshutdown)=
### `yshutdown`

Shut down all Yuneta processes on the local machine. Supports selective modes
(`--agent-only`, `--keep-agent`).

## Interactive Clients

Interactive, full-screen front-ends to a running yuno. (Both lived under
`yunos/c/` before 7.4.x — `yuno_cli` was renamed to `ycli` when it moved here.)

(util-ycli)=
### `ycli`

Interactive ncurses-based terminal UI for browsing yunos, inspecting state, and
sending commands. Multi-session / multi-window sibling of `ycommand`; both use
the shared `C_EDITLINE` line editor, so line editing, history, `Ctrl+R` /
`Ctrl+S` incremental search (`ESC` / `Ctrl+G` cancel) and TAB completion behave
identically in both. `ycli` renders the multi-candidate TAB list as an ncurses
popup above the editline (`Tab` / `Up` / `Down` navigate, `Enter` commits the
candidate to the line without submitting, `ESC` / `Ctrl+G` cancel); a
per-connection cache of remote commands is warmed on connect via
`list-gobj-commands`, so the candidate set follows whichever window has focus.
Local vs remote routing follows the `!cmd` prefix convention. `Ctrl+K` deletes
from cursor to end of line (readline); `Ctrl+U` / `Ctrl+Y` clear the whole line;
`Ctrl+L` clears the display pane.

(util-mqtt_tui)=
### `mqtt_tui`

Interactive MQTT client with a text-based UI — connects to an MQTT broker and a
Yuneta broker simultaneously, with interactive publish / subscribe / unsubscribe,
MQTT v5 property inspection, and optional OIDC auth (fetches a JWT and presents
it as the MQTT password). Speaks v3.1 / v3.1.1 / v5.0 via `C_PROT_MQTT2`.

## Data Inspection

(util-tr2list)=
### `tr2list`

List records from a timeranger2 topic. Supports time-range filtering, field
selection, metadata display, and local timestamps.

(util-tr2keys)=
### `tr2keys`

List all keys (primary ids) stored in a timeranger2 topic.

(util-tr2search)=
### `tr2search`

Search records inside a timeranger2 topic by content filters and key, with
optional base64 decoding.

(util-tr2migrate)=
### `tr2migrate`

Migrate data from legacy timeranger (v1) to timeranger2.

(util-treedb_list)=
### `treedb_list`

List nodes of a TreeDB graph database — walks topics and their hook/fkey
relationships.

(util-msg2db_list)=
### `msg2db_list`

List messages from a msg2db database (dict-style message store on timeranger2).

(util-list_queue_msgs2)=
### `list_queue_msgs2`

List messages from a msg2db persistent queue.

(util-stats_list)=
### `stats_list`

List persisted statistics with filtering by group, variable, metric, units, and
time range.

(util-json_diff)=
### `json_diff`

Semantic diff of two JSON files — supports unordered array comparison and
metadata/private-key filtering.

## Code Generation

(util-yuno-skeleton)=
### `yuno-skeleton`

Create a new yuno from a pre-defined template. Use `--list` to see available
skeletons.

(util-ymake-skeleton)=
### `ymake-skeleton`

Generate a yuno skeleton from an existing project, renaming identifiers to match
a new role.

(util-yclone-gclass)=
### `yclone-gclass`

Clone a GClass template under a new name — rewrites file names and identifiers
automatically.

(util-yclone-project)=
### `yclone-project`

Clone a whole Yuneta project (yuno, util, …) under a new name.

(util-yscapec)=
### `yscapec`

Escape a file's contents into a C string literal. Handy for embedding JSON, SQL,
or HTML in `.c` source.

## Diagnostics

(util-fs_watcher)=
### `fs_watcher`

Test harness for the `fs_watcher` library — prints every filesystem event on a
path.

(util-inotify)=
### `inotify`

Raw Linux `inotify` diagnostic tool — decodes and prints inotify event flags.

(util-ytestconfig)=
### `ytestconfig`

Validate a JSON configuration file — exits non-zero on parse failure.

(util-time2date)=
### `time2date`

Bidirectional converter between Unix timestamps and ASCII date/time strings.

(util-time2range)=
### `time2range`

Convert a Unix timestamp into a human-friendly time range (seconds → years).

(util-static_binary_test)=
### `static_binary_test`

Smoke test for fully-static builds (`CONFIG_FULLY_STATIC`).

## Security

(util-keycloak_pkey_to_jwks)=
### `keycloak_pkey_to_jwks`

Convert an RSA private key (base64 DER) to a JWKS document for OAuth 2 / JWT
providers.
