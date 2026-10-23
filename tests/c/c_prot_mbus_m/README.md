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
- **slave `12345678` KAM** (secondary) is selected every cycle (`CI 52`, the
  bytes checked) and read at `FD`;
- **slave 3** is disabled and must never be asked.

At the end the counters must be what three cycles give: `badFrames` 1,
`timeouts` 4, `repeats` 3, `failedReads` 2, `readings` 7.

The master is checked to fail this test when it does not toggle the FCB, when
it logs a failing slave on every cycle, or when it toggles the FCB on a
repeat.

The test is built only when `CONFIG_MODULE_MBUS` is set.

## Run

```bash
ctest -R '^c_prot_mbus_m/' --output-on-failure --test-dir build
```
