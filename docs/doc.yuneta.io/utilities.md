# Utilities

Command-line tools shipped with Yuneta. `yunetas build` installs these binaries
into `/yuneta/bin/`, which [`yunetas-env.sh`](https://github.com/artgins/yunetas/blob/7.5.12/yunetas-env.sh) puts on your `PATH` with top
priority — so every tool below is runnable by name from any directory.

**Source:** `utils/c/`

See also [Tools](tools.md) for the build infrastructure (`tools/cmake/`) and the
agent helper scripts (`tools/agent/`).

Each tool has its own page (see the navigation sidebar). Grouped by area:

## Control Plane

- [`ycommand`](utilities/ycommand.md)
- [`ybatch`](utilities/ybatch.md)
- [`ystats`](utilities/ystats.md)
- [`ytests`](utilities/ytests.md)
- [`ylist`](utilities/ylist.md)
- [`yshutdown`](utilities/yshutdown.md)

## Interactive Clients

Interactive, full-screen front-ends to a running yuno. (Both lived under
`yunos/c/` before 7.4.x — `yuno_cli` was renamed to [`ycli`](#util-ycli) when it moved here.)

- [`ycli`](utilities/ycli.md)
- [`mqtt_tui`](utilities/mqtt_tui.md)

## Data Inspection

- [`tr2list`](utilities/tr2list.md)
- [`tr2keys`](utilities/tr2keys.md)
- [`tr2search`](utilities/tr2search.md)
- [`tr2migrate`](utilities/tr2migrate.md)
- [`treedb_list`](utilities/treedb_list.md)
- [`msg2db_list`](utilities/msg2db_list.md)
- [`list_queue_msgs2`](utilities/list_queue_msgs2.md)
- [`stats_list`](utilities/stats_list.md)
- [`json_diff`](utilities/json_diff.md)

## Code Generation

- [`yuno-skeleton`](utilities/yuno-skeleton.md)
- [`ymake-skeleton`](utilities/ymake-skeleton.md)
- [`yclone-gclass`](utilities/yclone-gclass.md)
- [`yclone-project`](utilities/yclone-project.md)
- [`yscapec`](utilities/yscapec.md)

## Diagnostics

- [`fs_watcher`](utilities/fs_watcher.md)
- [`inotify`](utilities/inotify.md)
- [`ytestconfig`](utilities/ytestconfig.md)
- [`time2date`](utilities/time2date.md)
- [`time2range`](utilities/time2range.md)
- [`static_binary_test`](utilities/static_binary_test.md)

## Security

- [`keycloak_pkey_to_jwks`](utilities/keycloak_pkey_to_jwks.md)
