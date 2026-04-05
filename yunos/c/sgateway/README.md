# sgateway

Simple bidirectional gateway yuno. Forwards data between an input URL and an output URL (TCP or WebSocket), with optional TLS. Useful for quick protocol bridging, tunnelling, or fan-in/fan-out scenarios without writing a dedicated yuno.

## Configuration

Set `input_url` and `output_url` in the yuno config; enable TLS by using `wss://` / `tcps://` schemes.
