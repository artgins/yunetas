(util-ycommand)=
# `ycommand`

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
