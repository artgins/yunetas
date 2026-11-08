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

`test6` (`main_test6.c` + `c_test6.c`) is a `C_WEBSOCKET` **server** with
2 channels and a `timeout_handshake` of 1 s. Two raw clients connect and send
ONE byte each, `G`, and no more: the server must close both at
`timeout_handshake` (warning *"Timeout waiting websocket handshake"*, not
before 0.9 s nor after 5 s). Then a third raw client sends a whole upgrade
and gets its `101`: the channels are free again. In that session it sends a
text frame not final and another text frame not final, masked: the server
closes it with ONE warning, *"Websocket data frame inside a fragmented
message, connection closed"*. Up to 7.30.15 every read stopped the timer of
the handshake and nothing armed it again: the two clients held their
connections, and the pool, for ever (red: *"A half handshake held its
connection"*); and the bad frame was closed with no log (red).

`test7` (`main_test7.c` + `c_test7.c`) is what a `C_WEBSOCKET` **client**
(`timeout_handshake` 1 s, `timeout_close` 0.5 s) closes for. A raw server
answers one case per connection, and the client reconnects:

| The server writes | The client |
|---|---|
| half a `101`, and nothing more | closes at `timeout_handshake`, ONE warning |
| a right `101` and `c1 02 "hi"` (RSV1) | closes, ONE warning, then *"Timeout waiting websocket disconnected"* |
| a right `101` and `80 02 "hi"` (a final continuation, no message begun) | the same |
| a right `101` and `00 02 "hi"` (a continuation not final) | the same |
| a right `101` and `41 01 "a"` (RSV1, a text frame not final) | the same |
| a right `101` and `09 02 "hi"` (a ping not final) | the same |
| a right `101` and `81 02 c3 28` (a text frame not utf-8) | closes with `1007`, ONE warning, then the disconnect timeout |
| a right `101` and `89 7e 00 7e` + 126 bytes (a ping of 126 bytes) | closes, ONE warning, then the disconnect timeout |
| `Upgrade: websocket `, `Connection:\t Upgrade \t`, `Sec-WebSocket-Accept: <accept> ` and `81 05 "hello"` | opens the session and delivers `hello`; the server drops it |
| a right `101`, `01 01 "a"` and `81 01 "b"` (data inside a fragmented message) | closes, ONE warning, then the disconnect timeout: the test ends |

Up to 7.30.15 the client waited for the rest of the half `101` for ever (red:
*"Not every case was answered"*), the bad frames were closed with no log, a
text frame not final with an RSV bit was taken, and the `101` with OWS was
refused (red: *"The frame of the 101 with OWS was not delivered"*). A text
frame not final inside a fragmented message is test6's.

`test8` (`main_test8.c` + `c_test8.c`) is a message that must not cross from
one connection to the next. The `C_WEBSOCKET` server has ONE channel. Raw
client A upgrades, sends `01 88 <key> "A-secret"` (a text frame not final,
masked) and a ping; at the pong (the server read the fragment) it leaves.
Raw client B then upgrades on the same channel and sends `80 81 <key> "b"`, a
final continuation: the server closes it with ONE warning, *"Websocket
continuation frame with no message begun, connection closed"*, and delivers
no message. Up to 7.30.15 the fragment stayed in the gobj, and B's session
was delivered `A-secretb` (red: *"The second connection got a message with
the bytes of the first one"*).

The raw servers of test1, test2, test3, test5 and test7 answer the
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
