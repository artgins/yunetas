# c_prot_modbus_m test

Tests `C_PROT_MODBUS_M`, the Modbus master: `test1` over TCP, `test2` with
RTU framing.

`test1` (`main_test1.c` + `c_test1.c`) is about what a polling cycle
publishes. A raw server (`C_PROT_RAW`) answers Modbus TCP by hand to a master
that polls two slaves: slave 1 with blocks A (`a`, `uint32`) and B (`b`), and
slave 2 with block C (`c`).

- Cycles 1 and 2: slave 2 answers exception 11, and block B exception 2 in
  cycle 1 and the echo of a write (06) in cycle 2. Only `{slave_id 1, a}` is
  published, at once (within 800 ms of the request of block A), and nothing
  of slave 2. Each failure is logged once, as a warning, not on every cycle;
  the 06 to block B fails it (*"Modbus answer of another function"*). Before
  the fix of REV-CLOUD V46.2 it counted as the answer of the block.
- On the message of cycle 1 a write (function 06) is queued: the slave must
  see it once, and the polling must go on after it.
- With it, four more requests, each of which must be answered with
  `EV_MODBUS_ANSWER`, in order, with what the slave said: a write of three
  registers (function 16, checked at the slave with its quantity and byte
  count), a read of them back, a write the slave refuses with exception 1,
  and a coil (function 05, ON is `0xFF00`).
- A request sent before the master connects is answered at once, as failed
  (*"not connected"*).
- The `traffic` trace of the master and of its `C_TCP` is armed, and
  `main_test1.c` scans every log line, traces included: two write values made
  to be found (`0x7E5F` with function 06, `0x5A3C 0x6B4D` with 16) must never
  show. The one line allowed is the transport's dump of what it receives
  (`⏪`): the echo of an 06 carries its value, and received bytes cannot be
  marked secret.
- (REV-CLOUD V45) The requests go out at once on the message of cycle 1, not
  a `timeout_polling` later. Requests whose numbers are not numbers
  (`"abc"`, `null`, `"010"`, `20.9`, `"12ab"`, an id `"x"`), or that pass
  65535, are refused, and the refusal of a value that does not fit does not
  show it. A read the slave answers only after the next request went out
  (`timeout_response` first) is dropped by its transaction id instead of
  answering the next one. An answer with an MBAP of length 3 fails its read
  as a bad answer (a warning), not as an internal error.
- (REV-CLOUD V46.2) An exception `0x84` to a read (03) fails it as a bad
  answer (`error_code` 0), not as exception 2. A write (06) of `0x7E5F` is
  answered late, while the next read is in flight: its echo is dropped, and
  the master's own `traffic` dump of it is hidden (before the fix it showed
  the value). An answer followed in the same bytes by half of a stray frame,
  whose other half comes before the next answer: the stray frame is read
  whole and dropped (`in_flight` 0), and both answers are taken (before the
  fix the half was dropped and the next answer lost). A head with another
  transaction id and an MBAP length of 65535, before an answer: the master
  drops the connection, the read fails with *"the connection dropped before
  its answer"*, and the master comes back (before the fix the head swallowed
  the answer, and everything until `timeout_response`).
- Cycle 3: every block answers, everything is published, and each block that
  was failing is logged as *"Modbus block answered again"*.

Against SDK 7.29.2 it fails on the requests: none is answered, and the write
of three registers reaches the slave without its quantity. Against SDK 7.27.7
it fails on all three defects fixed after it: `b` and `c`
published as 0 (*"The master published what the cycle did not read"*), each
exception waiting `timeout_response` for a CRC (*"A cycle with exceptions did
not end at once"*), and the write released while queued (*"BAD
json_decref()"*, then a crash).

`test2` (`main_test2.c` + `c_test2.c`) gives the master `"modbus_protocol":
"RTU"` over its own `C_TCP` (as a gate does): the raw server answers RTU
frames by hand, with a CRC computed bit by bit. On the message of cycle 1 it
queues writes of one and three registers (06, 16), one and nine coils (05,
15), reads them back (03, 01), a write the slave refuses with exception 1,
and two answers with a bad CRC: an exception and a read. Each must be
answered in order; the two bad CRCs fail their request with a warning
(*"Modbus answer with a bad CRC"*). Before the fix of REV-CLOUD V45.2 the
exception was taken before its CRC was checked, and the read logged an
internal error (*"answered WITHOUT reading its answer"*).

(REV-CLOUD V46.2) A request to slave 248 is refused (248-255 are reserved in
RTU), and slave 250 of the config is never polled (one error at start, and
*"cur_map_ DISABLED"* each cycle). A read the slave never answers times out,
and its exception (`0x83`) comes late, while the next request, a write (06),
is in flight: it is dropped, and the echo that follows answers the write.
Before the fix the request to 248 and the polls of 250 went out, and the late
exception failed the write, whose echo was then dropped.

The tests are built only when `CONFIG_MODULE_MODBUS` is set.

## Run

```bash
ctest -R '^c_prot_modbus_m/' --output-on-failure --test-dir build
```
