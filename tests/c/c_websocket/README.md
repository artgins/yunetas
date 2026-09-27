# c_websocket test

Tests for the websocket GClass `C_WEBSOCKET`.

`test1` (`main_test1.c` + `c_test1.c`) closes a websocket **client** in the
middle of a frame and sends it the rest afterwards. The server is raw
(`C_PROT_RAW`) and plays websocket by hand: it answers the upgrade, sends the
header of a 1000-byte frame with 10 bytes of it, and the other 990 bytes
2.5 s later. The client's `timeout_payload` (1 s) closes it first: `ws_close()`
moves it to `ST_DISCONNECTED` and sends its Close frame, and a client keeps the
connection until the server drops it or `timeout_close` runs out -- RFC 6455
lets an endpoint that sent a Close go on receiving. The late bytes must be
taken as nothing: no error, no message delivered; the client drops at
`timeout_close` and the test ends. Up to 7.25.10 `ST_DISCONNECTED` did not
declare `EV_RX_DATA`: *"Event NOT DEFINED in state"* (red).

## Run

```bash
ctest -R '^c_websocket/' --output-on-failure --test-dir build
```
