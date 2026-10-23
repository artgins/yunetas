# modules/c/mbus

M-Bus master (EN 13757-2/-3) for wired meter buses: water, heat, gas and
electricity meters, read through a serial M-Bus level converter (`C_UART`,
2400 baud 8E1 by default) or a transparent M-Bus to TCP gateway (`C_TCP`).

| File | What it is |
|---|---|
| `src/mbus_telegram.c/.h` | The telegram decoder: no gobj, no state. Finds frames, builds the requests of a master, and decodes the variable data structure to json, with one unit per quantity and record ids that stay the same from one reading to the next. Never logs: a failure comes back with its reason, for the caller to log |
| `src/c_prot_mbus_m.c/.h` | `C_PROT_MBUS_M`, the master gclass (child model). Reads each slave by primary or secondary address, follows multi-telegram readouts with the FCB, retries, publishes one `EV_ON_MESSAGE` per slave read, and logs a failing slave on transition, not on every cycle. Commissioning commands: `scan-primary`, `scan-secondary`, `set-primary-address` (operations that run when the bus is free; `operation-status` gives their result), `read-now`, `dump-data` |

A minimal master on a serial level converter:

```C
hgobj gobj_mbus = gobj_create("bus1", C_PROT_MBUS_M,
    json_pack("{s:s, s:[{s:i}, {s:s}]}",
        "url", "/dev/ttyUSB0",
        "slaves",
            "address", 1,
            "secondary", "12345678"
    ),
    gobj
);
gobj_start(gobj_mbus);
```

Enabled by `CONFIG_MODULE_MBUS` (`default y`); link `${MODULE_MBUS}`. After
pulling this module into an existing checkout, add the symbol to `.config`
(`olddefconfig`, or `menuconfig` and save) and run `yunetas init`.

Reference, with the configuration, what a reading carries, the record ids,
the units and every function of `mbus_telegram.h`:
[doc.yuneta.io/mbus](https://doc.yuneta.io/mbus) and
[doc.yuneta.io/mbus-telegram](https://doc.yuneta.io/mbus-telegram).

Tests: `tests/c/mbus_telegram` (the decoder, with real telegrams checked
against libmbus) and `tests/c/c_prot_mbus_m` (the master against a simulated
bus: `test1` the cycle, `test2` the operations).
