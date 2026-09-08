# Logging API

The logging and tracing infrastructure: structured log emission, runtime
trace levels, the UDP handler that ships logs to logcenter, and the rotatory
file backend.

## In this section

- [Log](log.md) — `gobj_log_*()` structured logging.
- [Trace](trace.md) — runtime trace levels (global, per-gclass, per-gobj).
- [Log UDP Handler](log_udp_handler.md) — shipping logs over UDP.
- [Rotatory](rotatory.md) — the rotating-file log backend.
