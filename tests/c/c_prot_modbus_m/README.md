# c_prot_modbus_m test

Tests `C_PROT_MODBUS_M`, the Modbus master: `test1` over TCP, `test2` with
RTU framing.

`test1` (`main_test1.c` + `c_test1.c`) is about what a polling cycle
publishes. A raw server (`C_PROT_RAW`) answers Modbus TCP by hand to a master
that polls two slaves: slave 1 with blocks A (`a`, `uint32`) and B (`b`), and
slave 2 with block C (`c`).

- Cycles 1 and 2: block B answers exception 2 and slave 2 exception 11. Only
  `{slave_id 1, a}` is published, at once (within 800 ms of the request of
  block A), and nothing of slave 2. Each exception is logged once, as a
  warning, not on every cycle.
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

The tests are built only when `CONFIG_MODULE_MODBUS` is set.

## Run

```bash
ctest -R '^c_prot_modbus_m/' --output-on-failure --test-dir build
```
