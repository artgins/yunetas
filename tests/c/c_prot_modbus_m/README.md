# c_prot_modbus_m test

Tests `C_PROT_MODBUS_M`, the Modbus master: `test1` over TCP, `test2` and
`test3` with RTU framing, `test4` over its own `C_UART` on a pty. And `test5`
tests the `C_UART` alone, on a pty.

`test1` (`main_test1.c` + `c_test1.c`) is about what a polling cycle
publishes. A raw server (`C_PROT_RAW`) answers Modbus TCP by hand to a master
that polls two slaves: slave 1 with blocks A (`a`, `uint32`) and B (`b`), and
slave 2 with block C (`c`). Between A and B slave 1 has a block whose address
is out of range: it is logged once at start, and every cycle skips it with no
log and no wait (before, each cycle logged *"cur_map_
DISABLED"* and waited `timeout_polling` there, so no cycle ended within its
800 ms).

- Cycles 1 and 2: slave 2 answers exception 11, and block B exception 2 in
  cycle 1 and the echo of a write (06) in cycle 2. Only `{slave_id 1, a}` is
  published, at once (within 800 ms of the request of block A), and nothing
  of slave 2. Each failure is logged once, as a warning, not on every cycle;
  the 06 to block B fails it (*"Modbus answer of another function"*). Up to
  7.29.3 it counted as the answer of the block.
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
- The requests go out at once on the message of cycle 1, not
  a `timeout_polling` later. Requests whose numbers are not numbers
  (`"abc"`, `null`, `"010"`, `20.9`, `"12ab"`, an id `"x"`), or that pass
  65535, are refused, and the refusal of a value that does not fit does not
  show it. A read the slave answers only after the next request went out
  (`timeout_response` first) is dropped by its transaction id instead of
  answering the next one. An answer with an MBAP of length 3 fails its read
  as a bad answer (a warning), not as an internal error.
- An exception `0x84` to a read (03) fails it as a bad
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
(*"Modbus answer with a bad CRC"*). Up to 7.29.2 the
exception was taken before its CRC was checked, and the read logged an
internal error (*"answered WITHOUT reading its answer"*).

A request to slave 248 is refused (248-255 are reserved in
RTU). Slave 3 of the config has no block and slave 250 a reserved id: each is
logged once at start and never polled, and no cycle logs anything for them
(before, each cycle logged *"slave without mapping"* and *"cur_map_ NULL"*
for 3, and *"cur_map_ DISABLED"* for 250). A read the slave never answers times out,
and its exception (`0x83`) comes late, while the next request, a write (06),
is in flight: it is dropped, and the echo that follows answers the write.
Before the fix the request to 248 and the polls of 250 went out, and the late
exception failed the write, whose echo was then dropped.

Two reads are answered with noise after the answer, in the
same bytes: `0x00`, and `0x00 0xFF`. The noise is dropped with a warning
(*"Modbus: bytes after the answer, dropped"*), and the read sent after each is
answered. With `226945f9c` (never released) the noise began a frame: after
`0x00` the next answer was dropped as *"not the answer awaited"*, after
`0x00 0xFF` the master waited for more bytes until `timeout_response`. Slave 1
has a block B that the slave answers with 2 registers where it asked 1: it is
logged once (*"Quantity not corresponding to the request"*), not in every
cycle. Slave 0, the RTU broadcast, has a block in the config: it is reported
once at start and never polled (before, it was polled and every cycle waited
`timeout_response`). On the message of cycle 1 the slaves are written as `[]`
while the master runs: a warning, and cycle 2 polls the map it started with
(before, the write freed the map the cycle pointed into, and the master read
freed memory: *"cur_slave_ NULL"* every cycle, with `slaves_` read as a string).
The answer of block B, the last of the cycle, carries a noise byte in cycles 2
and 3 too: the noisy answers are logged once, and cycle 4, clean, logs
*"Modbus: a cycle came without noise"*. Before, each noisy answer was a
warning; and in a development version, never released, the noise after the last
block was counted after its cycle had ended, so the info came a cycle late, after
the end of the test. Two reads whose slave sends a noise
byte, `0x00` and `0xFF`, in a read of its own (a `C_TIMER0` holds the answer
50 ms) before the answer: neither can start an RTU answer, so both are
dropped and counted, and each read is answered. Before, the byte was taken as
the head of a frame, which swallowed the answer: each read timed out.
`rx_noise_dropped` is 6 at the end. Cycle 4 ends the test.

`test3` (`main_test3.c` + `c_test3.c`) gives an RTU master a
map with only slave 0, the broadcast: nothing to poll, so every tick of
`timeout_polling` ends a cycle that polled nothing, as a master that only
serves requests does. Three reads, three seconds apart, are each answered
with a noise byte after the answer in the same bytes: one warning for the
three (before, a warning and an info *"a cycle came without..."* for each, as
the empty cycles between them ended the noise), and on the answer of read n
`rx_noise_dropped` is already n (before, the noise was counted after the
answer was published). On the answer of the last read the test stops the
master inside its `EV_MODBUS_ANSWER`: nothing more is polled or logged
(before, *"cur_slave_ NULL"*, and an `EV_TIMEOUT` *"NOT DEFINED"* in
`ST_DISCONNECTED` at the next tick).

Between the first read and the other two, three requests
to slave 0, the RTU broadcast, with `timeout_response` at 4 s. A read from
slave 0 is refused at once (*"Modbus request refused"*). A write to slave 0
that the slave does not answer, as the standard says, is answered as done
(*"broadcast: no answer expected"*) within 2.5 s, after the turnaround delay.
A write to slave 0 that the slave echoes, as a device that breaks the
standard does: the echo is dropped as noise, the write is answered as done,
and the read after it is answered. Before the fix the read went out (the
slave saw a request it did not expect) and failed at `timeout_response`; both
writes failed there too; and the `0x00` of the echo was dropped, and its
`06 00 50` taken as the head of a frame of 82 bytes (*"Modbus frame dropped:
not the answer awaited"*, `slave_id` 6).

The master of `test3` has a `baudrate` of 1200, as behind
a `tcp://` gateway to a slow bus. A write of 123 registers to slave 0 (255
bytes, 2338 ms on that line) must be answered as done no sooner than 2.5 s
(its line time and the turnaround delay) and within 4.5 s. Before the fix the
turnaround delay was counted from before the frame was sent, and the write was
answered at the first tick after 200 ms, with its frame still on the line
(*"A broadcast write was answered with its frame still on the line"*). The
echo of a broadcast is warned under its own name (*"Modbus: bytes during a
broadcast, dropped..."*), not as the noise of the line.

`test4` (`main_test4.c` + `c_test4.c`) gives the master,
`"RTU"`, the slave side of a pty as its `url`: it creates its own `C_UART`.
The test answers RTU frames by hand on the master side of the pty, keeps a fd
of its own on the slave side, and counts the fds of the process open on that
path. On the message of the first cycle, published inside the read of the
`C_UART`, it stops the master there: the `C_UART` must close its tty at once
and end in `ST_STOPPED`. Started again, the master connects with one fd, not
two, and polls. Then it is stopped from a timeout, with the read in flight:
the `C_UART` goes to `ST_WAIT_STOPPED`, and ends in `ST_STOPPED` with the tty
closed; started again, it polls. The `traffic` trace of `C_UART` is on: it
dumps what it reads. Against SDK 7.29.3 the first stop left the tty open
(*"The fds open on the tty are not the expected"*), and the read callback of
the stopped `C_UART` answered -1 to the event loop, which ended the loop: the
yuno went down in the middle of the test.

`test5` (`main_test5.c` + `c_test5.c`) hosts a `C_UART` on the slave side of
a pty, and plays the other side of the line on the master side: it reads it,
or stops reading it to fill the line. It counts the fds open on the tty, and
the events the `C_UART` publishes to it.

- A write of 64 KB, more than the line takes at once: all of it arrives, in
  order, and `EV_TX_READY` comes once. Against SDK 7.29.3 the write was short
  and its rest lost: 11,776 bytes arrived, then `EV_TX_READY` (*"A phase did
  not end"*, `rx_count` 11776).
- `EV_DROP`: `EV_DISCONNECTED` inside the send, the tty closed, and opened
  again after `timeout_between_connections` (500 ms here). 7.29.3 had no
  `EV_DROP` in `C_UART`.
- The line not read, 64 KB and 8 bytes sent (no `EV_TX_READY`: the write is
  held), and a stop: 300 ms later the `C_UART` is in `ST_STOPPED` with its
  tty closed, and `EV_DISCONNECTED` and `EV_STOPPED` came. Then started
  again; the same held write, with one write more than `max_tx_queue` (1:
  the oldest queued is dropped, *"uart tx queue full, the oldest write
  dropped"*), a stop and a destroy right after it; and the line is read
  again, so the canceled write would complete if it were still there.
- A stop with the read in flight (`ST_WAIT_STOPPED`) and a destroy there: the
  tty is closed. Against 7.29.3 the fd stayed open (*"The fds open on the tty
  are not the expected"*).
- A stop with the read in flight and a `gobj_start()` there: refused with -1
  and *"Initial wrong task state"*, and the stop ends in `ST_STOPPED` with
  `EV_STOPPED`.
- A host that stops the `C_UART` in its `EV_RX_DATA` and destroys it in the
  `EV_DISCONNECTED` of that stop (wrong, and the kernel says so: *"Destroying
  a gobj while its tree is publishing"*): no `EV_STOPPED` comes from the freed
  gobj. Without the guard the `C_UART` published `EV_STOPPED` from it (a
  SEGV under ASan).

The tests are built only when `CONFIG_MODULE_MODBUS` is set.

## Run

```bash
ctest -R '^c_prot_modbus_m/' --output-on-failure --test-dir build
```
