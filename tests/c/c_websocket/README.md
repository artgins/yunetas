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

`test2` (`main_test2.c` + `c_test2.c`) is about the size of a frame. The
client has a `max_payload_size` of 60000 bytes. The raw server sends a frame
of 60000 bytes -- exactly the max -- in three parts: the client must deliver
it whole (its payload buffer starts at 4 KB and grows with what arrives; up to
7.25.21 its max was the max payload, a gbuffer holds one byte less than its
max, and such a frame never ended). Then it sends only the
header of a frame of 1 MB: the client must warn (*"Websocket frame bigger than
the max payload, connection closed"*) and close with 1009, reserving nothing.
Up to 7.25.21 the length written in the header was reserved at once, and
there was no maximum but the max block of the yuno.

`test3` (`main_test3.c` + `c_test3.c`) is the Close a peer sends. The raw
server answers the upgrade and sends a Close frame with code 4002 and reason
`peer fault`; the client must log *"Websocket Close received"* with both, echo
the Close, and drop at `timeout_close` (the raw server does not drop). Up to
7.28.9 the code and the reason were read and logged by nobody, so a server saw
only its own drop (`Local dropping`) for every session a browser closed (red).

`test4` (`main_test4.c` + `c_test4.c`) is the subprotocol a `C_WEBSOCKET`
server answers. Five raw clients send an upgrade each, by hand: with no
`Sec-WebSocket-Protocol` the 101 names none; `chat, superchat` answers `chat`;
` , mqtt ,x` answers `mqtt`, the first value of the list; `bad value, chat` and
a token of 600 bytes answer none, with a warning. Up to 7.29.3 the 101 echoed
the whole header, and at 512 bytes it was cut along with the end of the head,
so the answer never ended (red). Two more clients test the exact limit: a
token of `NAME_MAX` bytes (255) is answered, one of 256 answers none, with a
warning (the buffer holds 255 and its NUL: with `>` for `>=` in
`select_subprotocol()` the 256 bytes are answered, and written one byte past
it). Every 101 must carry `Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=`,
the accept of the key of RFC 6455 (1.3) the clients send. A last client, which
waits for the others so that its warning comes last, sends a key of 72
characters: it is answered `400`, with the warning *"Websocket upgrade request
refused"* (up to 7.30.14 the key was cut at 64 characters where the accept is
computed, and answered a wrong accept: red).

`test5` (`main_test5.c` + `c_test5.c`) is the handshake a `C_WEBSOCKET`
**client** sends and accepts (RFC 6455 4.1). A raw server answers eight
upgrades, one per connection, and the client reconnects after each refusal.
Every `Sec-WebSocket-Key` it gets must be 16 bytes in base64, not all of them
hex digits, and not the key of the connection before:

| The 101 | The client |
|---|---|
| no `Sec-WebSocket-Accept` | refuses it, ONE warning |
| a wrong `Sec-WebSocket-Accept` | refuses it, ONE warning |
| the right one, and `Sec-WebSocket-Protocol: chat` (not offered) | refuses it, ONE warning |
| no `Upgrade` | refuses it, ONE warning |
| `Upgrade: h2c` | refuses it, ONE warning |
| no `Connection` | refuses it, ONE warning |
| `Sec-WebSocket-Extensions: permessage-deflate` (not asked for) | refuses it, ONE warning |
| `Upgrade: WebSocket`, `Connection: keep-alive, Upgrade`, the right accept, and a text frame `hello` in the same write | opens the session and delivers `hello`: the test ends |

Up to 7.30.14 the client looked only at the 101 and opened the session with
the first one (red: *"The client opened a session with a handshake it must
refuse"*), its key was the clock in hex (red: *"The Sec-WebSocket-Key is not
16 random bytes"*), and the frame behind the last 101 was dropped with the
rest of that read (red: *"Not every case of the handshake was answered"*).
Without the case-insensitive match of `Upgrade`, the last case is refused.

The raw servers of test1, test2, test3 and test5 answer the
`Sec-WebSocket-Accept` of the key the client sent, computed by
`ws_handshake.c` (the SHA-1 of `sha1.c` and `gbuffer_binary_to_base64()`).

`ws_accept` (`test_ws_accept.c`, no yuno) is the `Sec-WebSocket-Accept` a
`C_WEBSOCKET` server answers: the SHA-1 of `sha1.c` (root-linux, private,
written again from FIPS 180-4 after 7.28.4) against the FIPS vectors and
every length around the padding, and its base64 against the example of
RFC 6455 (1.3). A browser refuses a wrong accept, and so does the C client
(test5).

## Run

```bash
ctest -R '^c_websocket/' --output-on-failure --test-dir build
```
