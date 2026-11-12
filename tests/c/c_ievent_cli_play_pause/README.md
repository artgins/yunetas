# c_ievent_cli_play_pause test

Tests who may play and pause a yuno through a `C_IEVENT_CLI`
(`kernel/c/root-linux/src/c_ievent_cli.c`): only the link to the yuno's own
agent.

One yuno holds both sides: a raw websocket server on `ws://127.0.0.1:7897`
(`C_IOGATE` -> `C_TCP_S`, channels of `C_WEBSOCKET` -> `C_TCP`, no
`C_IEVENT_SRV`) whose frames the test writes by hand, and two clients
(`C_IEVENT_CLI` over `C_IOGATE` -> `C_CHANNEL` -> `C_WEBSOCKET` -> `C_TCP`):

1. `cli_other`, configured to reach a `central` (as a gate's link to a
   central, or the agent's link to its control center). Its server answers
   the identity card claiming to be a `yuneta_agent`, then sends
   `EV_PAUSE_YUNO` twice and `EV_PLAY_YUNO`. The three are refused: each ack
   carries `"result": -1` and a comment, the yuno keeps playing (the default
   service is never paused), and ONE warning is logged for the session
   (*"play/pause of the yuno refused: only the link to its agent plays or
   pauses it"*, `MSGSET_AUTH`). Then the server drops the session; the link
   comes back, and in its second session one `EV_PAUSE_YUNO` is refused
   again, with a second warning: the mark goes with the session.
2. `cli_agent`, configured to reach a `yuneta_agent` (the link the agent
   writes into the config of every yuno it launches). Started after the
   four refusals. Its `EV_PAUSE_YUNO` pauses the yuno: ack `"result": 0`,
   the yuno no longer playing, the default service paused once. Its
   `EV_PLAY_YUNO` plays it again: ack `"result": 0`, the yuno playing. That
   ends the test.

Up to 7.30.26 the first pause of `cli_other` paused the yuno.

Mutants killed: the check removed; the check reading the role the server
says (`remote_yuno_role`) instead of the configured one; the per-session
brake of the warning removed (three warnings); `EV_PLAY_YUNO` refused on
every link, the agent's too (no yuno launched by an agent would ever play);
the mark of the refusal not reset when the session closes (one warning for
two sessions).
