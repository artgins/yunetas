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
so the answer never ended (red).

`ws_accept` (`test_ws_accept.c`, no yuno) is the `Sec-WebSocket-Accept` a
`C_WEBSOCKET` server answers: the SHA-1 of `sha1.c` (root-linux, private,
written again from FIPS 180-4 after 7.28.4) against the FIPS vectors and
every length around the padding, and its base64 against the example of
RFC 6455 (1.3). A browser refuses a wrong accept; the C client does not check
it, so test1 and test2 (whose server is raw) would never see one.

## Run

```bash
ctest -R '^c_websocket/' --output-on-failure --test-dir build
```
