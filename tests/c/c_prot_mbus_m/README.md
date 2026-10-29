# c_prot_mbus_m test

Tests `C_PROT_MBUS_M`, the M-Bus master, over TCP: a raw server
(`C_PROT_RAW`) plays a transparent M-Bus gateway, and `C_TEST1` answers as the
meters of the bus (it parses the master's frames with `mbus_telegram.h`).

`test1` (`main_test1.c` + `c_test1.c`) reads four slaves for three cycles:

- **slave 1** (primary) answers `SND_NKE` and then two telegrams: the first
  ends with `DIF 1F`. The first `REQ_UD2` must carry FCB 1 (`7B`) and the
  second FCB 0 (`5B`); the reading published has the records of both;
- **slave 2** (primary) is silent in cycles 1 and 2: *"M-Bus slave not read"*
  is logged once, as a warning, in cycle 1 and not in cycle 2, and nothing is
  published for it. In cycle 3 its first `RSP_UD` has a bad checksum: the
  master waits out `timeout_response` and repeats the `REQ_UD2` with the SAME
  FCB, the slave answers it, the reading is published and *"M-Bus slave
  answered again"* is logged;
- **slave `12345678` KAM** (secondary) is selected every cycle (`CI 52` with
  FCB 1, `73`; the bytes checked) and read at `FD`;
- **slave 3** is disabled and must never be asked.

At the end the counters must be what three cycles give: `badFrames` 1,
`timeouts` 4, `repeats` 3, `failedReads` 2, `readings` 7.

The master is checked to fail this test when it does not toggle the FCB, when
it logs a failing slave on every cycle, or when it toggles the FCB on a
repeat.

`test2` (`main_test2.c` + `c_test2.c`) is about the operations, on a bus of
six meters: `11111111` at address 1, `33333333` at 3, `41111111` and
`42222222` both at 4, and `12345678` and `12349999` with no primary address.

- On the reading of slave 1 (the cycle is in progress) `scan-primary 1-5` is
  asked: it waits for the end of the cycle, and finds addresses 1 and 3 with
  their identity, and a collision at 4. A `scan-secondary` asked meanwhile is
  refused (one operation at a time).
- `scan-secondary` finds the six meters, in the order of the tree search.
- `set-primary-address` with a wildcard is refused. Then `12349999` gets
  address 9: the bus checks that the record written is `01 7A 09`, and the
  meter answers `SND_NKE` at 9. The meters are strict about the FCB: a
  `SND_UD` with the FCB of the one before is a repeat, acknowledged and not
  done, and a selection clears that memory. So the write takes only with
  FCB 1, as the first `SND_UD` after a selection must carry.
- `read-now` starts a cycle at once (`timeout_polling` is a minute), and a
  second `read-now` while it runs is refused.

Each operation must end with *"M-Bus operation done"*, logged once. The test
fails when a collision does not narrow the search, and when the write carries
another VIF.

`test3` (`main_test3.c` + `c_test3.c`) is about answers that take longer on
the wire than `timeout_response` (100 ms here), as a long frame does at 2400
baud (~1.2 s). The one meter, at address 1, sends its 246-byte `RSP_UD` in
chunks of 10 bytes every 30 ms (~750 ms):

- in cycle 1 the master must read it with no timeout and no repeat;
- in cycle 2 the first answer is broken at its first byte and the rest is
  dribbled all the same. The master must wait until the bus is quiet before it
  repeats the `REQ_UD2` (same FCB), and the repeat is answered at once;
- in cycle 3 the first answer is noise that never goes quiet: one bad byte
  every 80 ms, less than `timeout_response`. The master must give up on it
  in its time bound (`timeout_response` plus the wire time of two frames at
  2400 baud, ~2.5 s): the repeat must reach the bus 2.4-3.5 s after the
  first `REQ_UD2`, and it is answered at once. With the bound counted in
  bytes the noise held the request for ~42 s.

The bus fails the test when a request arrives while an answer is on the wire.
At the end: `badFrames` 2, `timeouts` 0, `repeats` 2, `readings` 3.

`test1` and `test3` fail when the auto-kill time (20 s) ends them before their
last cycle.

The tests are built only when `CONFIG_MODULE_MBUS` is set.

## Run

```bash
ctest -R '^c_prot_mbus_m/' --output-on-failure --test-dir build
```
