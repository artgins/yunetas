# logcenter

Centralised logging yuno. Aggregates logs from other yunos via UDP and file handlers, applies rotation and filtering, and exposes the collected data for inspection.

Typically paired with the `to_udp` log handler (`udp://127.0.0.1:1992`) in per-yuno configurations.
