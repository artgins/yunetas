(module-mbus)=
# M-Bus

**Kconfig:** `CONFIG_MODULE_MBUS` — **GClasses:** `C_PROT_MBUS_M`

M-Bus (Meter-Bus, EN 13757) master: reads the water, heat, gas and electricity
meters of a wired M-Bus. The module has two parts:

- [`mbus_telegram.h`](#module-mbus-telegram), the telegram decoder: frames in,
  json out, with no gobj and no state;
- `C_PROT_MBUS_M`, the master gclass built on it.

Source code:

- [`c_prot_mbus_m.h`](https://github.com/artgins/yunetas/blob/7.28.5/modules/c/mbus/src/c_prot_mbus_m.h)
- [`c_prot_mbus_m.c`](https://github.com/artgins/yunetas/blob/7.28.5/modules/c/mbus/src/c_prot_mbus_m.c)

## The bus, in short

M-Bus is a two-wire bus, with no polarity, that also **powers the meters**. The
master talks by moving the bus voltage, and a meter answers by moving the
current it draws. So a serial port alone cannot drive it: the master needs an
**M-Bus level converter**, on USB or RS-232, or a **transparent M-Bus to TCP
gateway** that passes the bytes as they come. For this gclass both are a
stream of bytes: a `C_UART` in the first case, a `C_TCP` in the second.

The bus runs at **2400 baud, 8E1** (8 bits, even parity, 1 stop bit) unless
it is set up otherwise. A long frame is up to 261 bytes, about 1.2 s on the
wire, so a cycle of 250 meters takes minutes. That is fine for meters of water
and gas.

A meter **describes its own data**. It answers with a list of records, and each
record carries its quantity, its unit and its scale. So one decoder serves
every manufacturer, and there is no register map to write, as there is in
Modbus.

## C_PROT_MBUS_M

(register_c_prot_mbus_m)=
### `register_c_prot_mbus_m()`

Registers the gclass. A yuno that reads a bus calls it with the rest of its
gclasses:

```C
int register_c_prot_mbus_m(void);
```

```C
static int register_yuno_and_more(void)
{
    int result = 0;
    result += register_c_prot_mbus_m();
    result += register_c_my_gate();
    return result;  // a gclass refused stops the yuno
}
```

The master is a **child** gclass: it publishes to its parent (or to its
`subscriber`), so the parent declares `EV_ON_OPEN`, `EV_ON_MESSAGE` and
`EV_ON_CLOSE` in its FSM.

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

### Configuration

The bottom is made from `url` when the parent gives none: a `C_TCP` for
`tcp://host:port`, else a `C_UART` with `baudrate`, `bytesize`, `parity` and
`stopbits`. `slaves` lists the meters to read: one by its **primary address**
(1-250), or one by its **secondary address**, the 8 digits of its
identification. A secondary address can also give the `manufacturer` (3
letters), the `version` and the `medium` (a name or a code), so that two
manufacturers with the same number are not taken for one meter.

```json
{
    "url": "/dev/ttyUSB0",
    "baudrate": 2400,
    "parity": "even",
    "timeout_polling": 60000,
    "timeout_response": 500,
    "retries": 2,
    "max_telegrams": 8,
    "slaves": [
        {"address": 1},
        {"address": 2, "disabled": true},
        {"secondary": "12345678"},
        {"secondary": "87654321", "manufacturer": "KAM", "medium": "water"}
    ]
}
```

The same bus behind a gateway:

```json
{
    "url": "tcp://192.168.50.20:10001",
    "slaves": [{"address": 1}, {"address": 7}]
}
```

| Attribute | Default | Meaning |
|---|---|---|
| `url` | | Serial device of the level converter, or `tcp://host:port` of a gateway |
| `baudrate`, `bytesize`, `parity`, `stopbits` | 2400, 8, `even`, 1 | Serial line, given to the `C_UART` |
| `slaves` | `[]` | The meters. They are read at start, so a change needs a stop and a start |
| `timeout_polling` | 60000 | Milliseconds from the end of one cycle to the start of the next |
| `timeout_response` | 500 | Milliseconds to wait for an answer. EN 13757-2 gives a meter about 190 ms at 2400 baud. The rest is margin for a gateway |
| `retries` | 2 | Times a request without a valid answer is sent again |
| `max_telegrams` | 8 | The most telegrams in one reading |

A slave that is not written correctly is **left out, with an error**: a primary
address outside 1-250 or repeated, a secondary that is not 8 digits (an `F`
is a wildcard digit), a medium the decoder does not know, or both an
`address` and a `secondary`.

### The cycle

The slaves are read one after the other:

```
primary:     SND_NKE -> E5,  REQ_UD2 -> RSP_UD  [REQ_UD2 -> RSP_UD ...]
secondary:   SND_UD select (CI 52) -> E5,  REQ_UD2 to FD -> RSP_UD  [...]
```

- **`SND_NKE`** resets the link of the slave, so its readout starts at the
  first telegram.
- **The FCB:** the first `REQ_UD2` carries FCB 1 (`7B`). When a telegram
  ends with `DIF 1F` (more records follow), the FCB is toggled and the next
  telegram is asked for, up to `max_telegrams`.
- **A request with no valid answer** is sent again with the **same** FCB, up
  to `retries` times. A slave that gets the same FCB twice sends its last
  answer again.
- **A frame that does not parse** is waited out until `timeout_response`
  before the request goes again. So the rest of a collision, or of noise, does
  not fall into the next answer.
- **After the last slave,** the master waits `timeout_polling` and starts
  again.

The master publishes `EV_ON_OPEN` when the transport connects, and starts a
cycle at once. On `EV_DISCONNECTED` it drops the reading in progress and
publishes `EV_ON_CLOSE`. The transport reconnects on its own, and a new cycle
starts then.

### What a reading carries

It publishes **one `EV_ON_MESSAGE` per slave read**, as soon as its reading is
complete. It does not wait for the end of the cycle, because a cycle can be
minutes long and the reading has to carry the time it was read.

```json
{
    "address": 1,
    "header": {
        "id": "06855817", "manufacturer": "KAM", "version": 8,
        "medium": "heat", "medium_code": 4, "access_no": 4, "status": 0
    },
    "records": {
        "energy":           {"value": 37351000, "unit": "Wh"},
        "volume":           {"value": 561.08, "unit": "m3"},
        "volume@s1":        {"value": 500.98, "unit": "m3"},
        "flow_temperature": {"value": 101.69, "unit": "C"},
        "datetime":         {"value": "2011-01-05T15:26"}
    },
    "telegrams": 1
}
```

A slave read by its secondary address carries `"secondary": "12345678"` in
place of `address`. `header`, the id of each record and the units are those of
[`mbus_decode_user_data()`](#mbus_decode_user_data). A reading in several
telegrams has the records of all of them, and a record whose id repeats an
earlier one takes the next `#n`.

**Only what was read.** A slave that was not read in a cycle publishes nothing
in that cycle. Its earlier reading is never sent again, and a meter that does
not answer never comes out as zeros.

### Failures

A slave whose read fails is logged as a **warning** (`msgset` `Protocol`):
*"M-Bus slave not read"*, with the slave and the reason. It is logged **when
the slave starts failing**, or starts failing in another way, **not on every
cycle**. When the slave is read again, an info says so, with what it had
failed with:

```json
{"msg": "M-Bus slave not read", "slave": "2", "reason": "no answer to SND_NKE",
 "detail": "", "bytes": ""}
{"msg": "M-Bus slave answered again", "slave": "2",
 "failed_with": "no answer to SND_NKE"}
```

| `reason` | When |
|---|---|
| `no answer to SND_NKE` / `to the selection` / `to REQ_UD2` | The step had no answer, with all its retries |
| `bad frame to ...` | The last attempt got bytes that are not a frame (`detail` says why, `bytes` shows the first 64) |
| `bad answer` | A valid frame, but not the one asked for: no `E5`, an `RSP_UD` of another slave, or no records |
| `undecodable` | An `RSP_UD` the decoder refuses: fixed data (`CI 73`), records encrypted, a record cut short |
| `application error` | The meter answered `CI 70` |

Bytes that arrive with no request in flight are dropped, with one warning
(*"M-Bus bytes with no request in flight, dropped"*) until the next valid
answer.

### Stats

| Stat | Counts |
|---|---|
| `txFrames` | Frames sent |
| `rxFrames` | Valid frames received |
| `badFrames` | Frames that do not parse, or are not the answer asked for |
| `timeouts` | Requests with no answer in `timeout_response` |
| `repeats` | Requests sent again |
| `readings` | Readings published |
| `failedReads` | Reads of a slave that failed |

They are the communication diagnostics a meter network is asked for (frames
sent and received, bad frames, retries), and they are read like any other
stat:

```bash
ycommand -c 'command-yuno id=<yuno> service=<gate> command=stats'
```

### Commands

**`dump-data`** gives the last reading of each slave, whether it is
`disabled`, and what it is `failing` with (`""` when it is read). It takes
`address` or `secondary` to give one slave only. A slave that does not exist
answers `-1` (*"M-Bus slave not found"*).

```C
json_t *resp = gobj_command(gobj_mbus, "dump-data",
    json_pack("{s:i}", "address", 1),
    gobj
);
```

**`read-now`** starts a cycle at once, without waiting for `timeout_polling`.
It answers `-1` when the bus is not between cycles (not connected, a cycle
or an operation in progress).

```bash
ycommand -c 'command-yuno id=<yuno> service=<gate> command=read-now'
```

### Operations: putting meters into service

Three commands work on the bus itself. A command checks its arguments,
**starts its operation and answers at once**. The operation runs when the bus
is free: at once between cycles, or at the end of the cycle in progress. The
cycle waits while it runs, and goes on `timeout_polling` after it. Only one
runs at a time; a second one answers `-1`. **`operation-status`** gives the
state (`waiting`, `running`, `done`, `failed`) and the result. The end is
logged: *"M-Bus operation done"* as an INFO, with the number found, or *"M-Bus
operation failed"* as a WARNING, with the reason. A bus that closes fails the
operation in progress.

- **`scan-primary from=1 to=250`** sends a `SND_NKE` to each address. To
  each one that answers it sends a `REQ_UD2`, to find out who the meter is.
  When the `E5` of several meters with the same address overlap, it is a
  collision.
- **`scan-secondary`** is the wildcard tree search. It selects with an `F`
  in every digit not yet fixed (`0FFFFFFF`, `1FFFFFFF`, ...). With no answer,
  it goes on to the next digit. With one `E5`, it sends a `REQ_UD2` to the
  selected meter. When several meters answer at once, the garbage is a
  collision, and the search fixes one more digit (`10FFFFFF`, `11FFFFFF`,
  ...). Each step waits up to `timeout_response`, so a bus of a few meters
  takes tens of seconds.
- **`set-primary-address secondary=<8 digits> [manufacturer=<3 letters>]
  new_address=<1-250>`**
  1. selects the meter, which must answer `E5`;
  2. writes it the record `01 7A <address>` (`SND_UD`, `CI 51`), which must
     answer `E5`;
  3. checks that the meter answers `SND_NKE` at the new address.

  A secondary address with a wildcard is refused: it could select more than
  one meter.

```bash
ycommand -c 'command-yuno id=<yuno> service=<gate> command=scan-secondary'
ycommand -c 'command-yuno id=<yuno> service=<gate> command=operation-status'
```

```json
{
    "operation": "scan-primary",
    "state": "done",
    "args": {"from": 1, "to": 5},
    "asked": 1792750000,
    "ended": 1792750003,
    "error": "",
    "found": [
        {"address": 1, "secondary": "11111111", "manufacturer": "KAM",
         "version": 1, "medium": "water", "medium_code": 7},
        {"address": 4, "collision": true}
    ]
}
```

An entry of `scan-secondary` has the `secondary` and the primary `address`
the meter answered with (0 when it has none). An entry that could not be told
apart carries `"collision": true`, and one that answered the probe but not
the `REQ_UD2` carries an `error`.

**Trace levels:** `messages` (each reading published), `traffic` (raw bytes,
both ways), `polling` (each step of the cycle and of the operations, what
each scan finds, and a slave with more telegrams than `max_telegrams`).

### What it does not do (yet)

- **Wireless M-Bus** (EN 13757-4, 868 MHz radio, usually encrypted).
- The **fixed data structure** (`CI 73`/`77`), an old format: refused as
  `undecodable`.
- **Writing to a meter**, other than its primary address.
- Telling apart, in a secondary search, two meters with the same 8 digits
  (by manufacturer, version or medium): they are reported as a collision.
