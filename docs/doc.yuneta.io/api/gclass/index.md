# Built-in GClasses

The Yuneta kernel provides **32 built-in GClasses** organized in layers.
Every GClass is a reusable component driven by a finite state machine (FSM)
that communicates through events.

## Layer overview

| Layer | GClasses | Purpose |
|-------|----------|---------|
| [Transport](transport.md) | C_TCP, C_TCP_S, C_UDP, C_UDP_S, C_UART | Low-level network and serial I/O. |
| [Protocol](protocol.md) | C_PROT_HTTP_CL, C_PROT_HTTP_SR, C_PROT_TCP4H, C_PROT_RAW, C_WEBSOCKET | Message framing and protocol parsing. |
| [Gateway](gateway.md) | C_CHANNEL, C_IOGATE, C_QIOGATE, C_MQIOGATE | Message routing, multiplexing, and persistent queuing. |
| [Inter-Event](ievent.md) | C_IEVENT_CLI, C_IEVENT_SRV | RPC-like communication between yunos. |
| [Data](data.md) | C_TRANGER, C_TREEDB, C_NODE, C_RESOURCE2 | Time-series, graph DB, and resource persistence. |
| [Auth](auth.md) | C_AUTHZ, C_AUTH_BFF, C_TASK_AUTHENTICATE | Authentication, authorization, and OAuth 2. |
| [Task & Timer](task_timer.md) | C_TASK, C_TIMER, C_TIMER0, C_COUNTER | Async task execution, timers, and counters. |
| [System](system.md) | C_YUNO, C_FS, C_PTY, C_OTA, C_GSS_UDP_S | Core runtime, file-system watcher, PTY, OTA updates. |
