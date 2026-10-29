# c_prot_modbus_m test

Tests `C_PROT_MODBUS_M`, the Modbus master, over TCP.

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
- Cycle 3: every block answers, everything is published, and each block that
  was failing is logged as *"Modbus block answered again"*.

Against SDK 7.29.2 it fails on the requests: none is answered, and the write
of three registers reaches the slave without its quantity. Against SDK 7.27.7
it fails on all three defects fixed after it: `b` and `c`
published as 0 (*"The master published what the cycle did not read"*), each
exception waiting `timeout_response` for a CRC (*"A cycle with exceptions did
not end at once"*), and the write released while queued (*"BAD
json_decref()"*, then a crash).

The test is built only when `CONFIG_MODULE_MODBUS` is set.

## Run

```bash
ctest -R '^c_prot_modbus_m/' --output-on-failure --test-dir build
```
