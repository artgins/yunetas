(module-modbus)=
# Modbus

**Kconfig:** `CONFIG_MODULE_MODBUS` — **GClasses:** `C_PROT_MODBUS_M`

Modbus protocol master — reads/writes device registers (coils, discrete inputs,
input/holding registers). For industrial IoT.

## C_PROT_MODBUS_M

Modbus master protocol — supports four object types:

| Object | Access | Size |
|--------|--------|------|
| Coils | read-write | 1 bit |
| Discrete Inputs | read-only | 1 bit |
| Input Registers | read-only | 16 bits |
| Holding Registers | read-write | 16 bits |

Address range `0x0000`–`0xFFFF`. Configuration-driven slave device
mapping with data-format conversion and multiplier support.

The master runs over [`C_TCP`](#gclass-c-tcp) (`"modbus_protocol": "TCP"`, the
default) or over `C_UART` (`"RTU"`). Without a bottom gobj it creates its own
from `url`: a `C_TCP` for TCP, a `C_UART` otherwise.

### Configuration

The `slaves` attribute is the whole map: one entry per slave (unit id), each
with the blocks to read (`mapping`, one request per block) and the variables
decoded from them (`conversion`). A 32-bit total power of a meter that keeps
its low word first, read over TCP:

```json
{
    "modbus_protocol": "TCP",
    "url": "tcp://192.168.50.11:502",
    "timeout_polling": 10000,
    "timeout_response": 3,
    "slaves": [
        {
            "id": 1,
            "mapping": [
                {"type": "holding_register", "address": 14336, "size": 8}
            ],
            "conversion": [
                {
                    "id": "p_total",
                    "type": "holding_register",
                    "address": 14336,
                    "format": "int32",
                    "endian": "little endian byte swap"
                }
            ]
        }
    ]
}
```

- **`mapping`**: `type`, `address`, `size` (registers, at most 125; or bits, at
  most 2000). Each block is one read request.
- **`conversion`**: `id` (the key in the published message), `type`,
  `address`, `format` (`int16`, `uint16`, `bool`, `int32`, `uint32`, `int64`,
  `uint64`, `float`, `double`, `string`), `endian`, `multiplier`, and for a
  string `length_bytes`. A variable whose cells no block reads is disabled at
  start, with an error.
- **`endian`** says the order of the bytes of a value of several registers.
  With the wire bytes `A B C D` (first register `AB`, second `CD`):

  | `endian` | Value | Typical of |
  |---|---|---|
  | `big endian` (default) | `ABCD` | the Modbus convention, high word first |
  | `little endian` | `DCBA` | |
  | `big endian byte swap` | `BADC` | |
  | `little endian byte swap` | `CDAB` | low word first (SATEC and many meters) |

  The forms with `_` (`little_endian_byte_swap`) are accepted too.
- **`multiplier`** has two behaviours: **1 or more is applied as an integer,
  and its fraction is dropped** (`2.5` multiplies by 2); **below 1 the result
  is a real computed in single precision** (`float`, about 7 significant
  digits), so an energy counter of 8 or more digits loses its last ones.
  Leave it at 1 and scale where the reading is consumed when that matters.

| Attribute | Default | |
|---|---|---|
| `modbus_protocol` | `"TCP"` | `TCP`, `RTU` or `ASCII` |
| `url` | `""` | `tcp://<host>:<port>`, or the serial device |
| `slaves` | `[]` | The map, above. Read at `mt_start` |
| `timeout_polling` | `1000` | Milliseconds between the end of a cycle and the next. The timer counts whole seconds (`C_TIMER`), so less than 1000 is 1 s |
| `timeout_response` | `10` | Seconds a slave has to answer one request |

### The polling cycle

On `EV_CONNECTED` the master publishes `EV_ON_OPEN` and polls 1 s later. A
cycle reads each block of each slave in order, one request at a time; a
response, an exception or `timeout_response` moves it to the next block.
After the last block of the last slave it publishes, and the next cycle starts
`timeout_polling` later. When the last request of a cycle ends in a timeout,
the next cycle starts `timeout_response` later instead: a slave that did not
answer is given that time again before it is polled. On `EV_DISCONNECTED` it publishes `EV_ON_CLOSE`; a
`C_TCP` reconnects on its own.

### What a message carries

At the end of each cycle it publishes **one `EV_ON_MESSAGE` per slave**: the
`slave_id` and one key per variable, decoded and multiplied.

```json
{"slave_id": 1, "p_total": 5790}
```

**Only what the cycle read.** A variable is published only when every cell it
spans was stored since the previous publication, that is, when its block was
answered in this cycle. A block that timed out or was answered with an
exception leaves its variables out of the message, and a slave none of whose
blocks answered is not published at all; each failure was logged where it
started (`Modbus Timeout`, `modbus exception`: see below). So a
subscriber can receive **a message with fewer variables than the map
declares**, and must not read a missing one as zero. The cells read before a
disconnection are discarded with it. (Up to SDK 7.27.7 every variable of every
slave was published each cycle: a slave that stopped answering repeated its
last values, and one that never answered came out as zeros.)

A consumer that depends on this refuses to build against an older module, with
the macro its header defines since this change:

```C
#include <c_prot_modbus_m.h>
#ifndef C_PROT_MODBUS_M_PUBLISHES_WHAT_IT_READ
#error "C_PROT_MODBUS_M publishes what it did not read: needs the SDK after 7.27.7"
#endif
```

### Exceptions and timeouts

An exception response, or a block that does not answer within
`timeout_response`, moves the cycle on at once to the next block. Over TCP the
frame ends at the exception code; RTU adds its CRC.

Each failure is logged as a **warning** (`msgset` `Protocol`: the slave
answered, or did not, and nothing is broken on our side), with the
`slave_id`, the exception code and name, and the `block` it answered. It is
logged **when the block starts failing**, or starts failing another way (a new
exception code, or a timeout after an exception), **not on every cycle**: a
unit id that does not exist answers the same exception every cycle, and that
is one fact. When the block answers again, an info says so, with what it
failed with:

```json
{"msg": "modbus exception", "slave_id": 4, "error_code": 2,
 "error_name": "ILLEGAL_DATA_ADDRESS",
 "block": {"type": "holding_register", "address": 256, "size": 12}}
{"msg": "Modbus block answered again", "slave_id": 4,
 "failed_with": "modbus exception 2"}
```

The exception and the timeout of a **request** (below) are logged every
time, with the request (`request`), not a block. (Up to SDK 7.27.7 the TCP
master waited after an exception for a CRC that TCP does not carry, so each
exception cost `timeout_response` and held the poll of every slave of the
connection; and every exception and timeout of every cycle was an error.)

### Requests: writes, and reads outside the map

A request is an `EV_SEND_MESSAGE` to the gobj: a write (the default), or a
read of registers or bits that are not in the polling map, once.

```C
gobj_send_event(gobj_modbus, EV_SEND_MESSAGE,
    json_pack("{s:i, s:s, s:i, s:[iii]}",
        "id", 1,
        "type", "holding_register",
        "address", 46208,
        "value", 1, 10, 0
    ),
    gobj
);
gobj_send_event(gobj_modbus, EV_SEND_MESSAGE,
    json_pack("{s:s, s:i, s:s, s:i, s:i}",
        "operation", "read",
        "id", 1,
        "type", "holding_register",
        "address", 46208,
        "size", 3
    ),
    gobj
);
```

| Key | | |
|---|---|---|
| `operation` | `"write"` | `write` or `read` |
| `id` | `1` | Slave id, 0-255 |
| `type` | | Write: `holding_register`, `coil`. Read: also `input_register`, `input_status` |
| `address` | | 0-65535, a number or its text |
| `size` | `1` | A read: how many (at most 125 registers, 2000 bits). A write takes it from its values |
| `value` | | A write: a number, or an array of them (at most 123 registers, 1968 coils). A register value is -32768..65535; a coil is ON if not 0 |

Anything else in the request travels back in its answer, so a caller can tag
it. What goes out: a read with function 01, 02, 03 or 04 by its type; a write
of one holding register with 06, of several with 16; one coil with 05 (ON is
`0xFF00`), several with 15. Over TCP and RTU; ASCII is not built.

**Every request is answered, with `EV_MODBUS_ANSWER`, also when it fails**:

```json
{"request": {"operation": "read", "id": 1, "type": "holding_register",
             "address": 46208, "size": 3},
 "result": 0, "comment": "", "error_code": 0, "values": [1, 10, 0]}
{"request": {"operation": "write", "id": 1, "type": "holding_register",
             "address": 46213, "size": 1, "value": [100]},
 "result": -1, "comment": "modbus exception 1 ILLEGAL_FUNCTION",
 "error_code": 1}
```

`request` comes with its defaults filled in and the value of a write as an
array; `result` is 0 when done, -1 when not, with the reason in `comment`, and
the Modbus exception, if the slave sent one, in `error_code`; `values` are the
registers of a read, or its bits as 0/1. A write is checked against the echo
the slave answers with (its address, and its value or quantity): an echo that
does not match fails it. A request that cannot be sent -- a bad key, an
unknown type, a size out of range -- is answered at once, failed, and logged
as an error (*"Modbus request refused"*). The gobj above must declare
`EV_MODBUS_ANSWER` in its FSM.

**When it goes out.** A request that arrives while the connection waits
between two cycles goes out at once, and the wait resumes after the requests;
one that arrives while a block or another request is in flight goes out after
it, before the next block. They go **one at a time, in the order they came**,
so a request that needs another before it in the same connection -- a meter
password, and the setup it unlocks -- is simply sent after it.

**A request is valid for the connection it was queued in.** When the
connection drops, the request in flight and every queued one are answered as
failed (*"the connection dropped..."*) and none of them is sent on the next
connection: whatever unlocked them on the old one, a password, is gone with
it. A request sent while there is no connection is answered at once
(*"not connected"*). A stop of the gobj drops them, with a warning that lists
them (*"Modbus writes NOT sent, the gobj stopped"*, and *"NOT confirmed"* for
the one in flight).

A consumer that relies on the answers refuses to build against an older
module:

```C
#include <c_prot_modbus_m.h>
#ifndef C_PROT_MODBUS_M_ANSWERS_REQUESTS
#error "C_PROT_MODBUS_M does not answer requests: needs the SDK after 7.29.2"
#endif
```

(Up to SDK 7.29.2 no request was answered and only writes existed: one
holding register over TCP went out right; function 05 sent the value as given
instead of `0xFF00`, several values went out without their quantity and byte
count, RTU refused every write, and writes queued when the connection dropped
were sent on the next one. Up to 7.27.7 a write was never taken out of the
queue, and the polling did not resume after it.)

### Commands

**Commands:** `dump_data` (current register snapshot), `set-poll-timeout`.

`dump_data` takes `slave_id` (an integer, `-1` all slaves), `address` and
`size` (`-1` all data). `address` and `size` are `DTP_JSON` since SDK 7.27.0:
a number or its text, as the register maps write them (`"address": "4096"`).
They were `DTP_STRING`, which the command parser now holds to a string, so a
number from a SPA would be refused. An `address` above `0xFFFF` answers `-1`
*"Address out of range"*.

```C
/*  the address as a register map writes it, the size as a number  */
json_t *resp = gobj_command(gobj_modbus, "dump_data",
    json_pack("{s:i, s:s, s:i}", "slave_id", 1, "address", "4096", "size", 16),
    gobj
);
```

**Trace levels:** `messages` (each published message, and the variables a
cycle left out), `traffic` (raw bytes), `polling`, `decode`, `send`.
