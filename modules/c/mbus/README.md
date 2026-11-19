# modules/c/mbus

This module is an M-Bus master (EN 13757-2/-3) for wired meter buses. It reads water, heat, gas and
electricity meters. It reads them through a serial M-Bus level converter (`C_UART`,
2400 baud 8E1 by default) or through a transparent gateway from M-Bus to TCP (`C_TCP`).

| File | What it is |
|---|---|
| `src/mbus_telegram.c/.h` | The telegram decoder. It has no gobj and no state. It finds frames, builds the requests of a master, and decodes the variable data structure to json. The json has one unit per quantity, and record ids that stay the same from one reading to the next. It never logs. It returns a failure with its reason, and the caller logs it |
| `src/c_prot_mbus_m.c/.h` | `C_PROT_MBUS_M`, the master gclass (child model). It reads each slave by primary or secondary address. It follows readouts of more than one telegram with the FCB, and it retries. It publishes one `EV_ON_MESSAGE` for each slave that it reads. It logs a failing slave on transition, not on every cycle. Commissioning commands: `scan-primary`, `scan-secondary`, `set-primary-address` (operations that run when the bus is free, and `operation-status` gives their result), `read-now`, `dump-data` |

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

`CONFIG_MODULE_MBUS` (`default y`) enables the module. Link `${MODULE_MBUS}`. If you
pull this module into an existing checkout, add the symbol to `.config`
(`olddefconfig`, or `menuconfig` and save). Then run `yunetas init`.

The reference gives the configuration, the contents of a reading, the record ids,
the units and every function of `mbus_telegram.h`:
[doc.yuneta.io/mbus](https://doc.yuneta.io/mbus) and
[doc.yuneta.io/mbus-telegram](https://doc.yuneta.io/mbus-telegram).

Tests: `tests/c/mbus_telegram` (the decoder, with real telegrams compared
with libmbus) and `tests/c/c_prot_mbus_m` (the master against a simulated
bus: `test1` the cycle, `test2` the operations).
