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
| `timeout_polling` | `1000` | Milliseconds between the end of a cycle and the next |
| `timeout_response` | `10` | Seconds a slave has to answer one request |

### The polling cycle

On `EV_CONNECTED` the master publishes `EV_ON_OPEN` and polls 1 s later. A
cycle reads each block of each slave in order, one request at a time; a
response, an exception or `timeout_response` moves it to the next block.
After the last block of the last slave it publishes, and the next cycle starts
`timeout_polling` later. On `EV_DISCONNECTED` it publishes `EV_ON_CLOSE`; a
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
happened (`Modbus Timeout`, `modbus exception` with its code and block). So a
subscriber can receive **a message with fewer variables than the map
declares**, and must not read a missing one as zero. The cells read before a
disconnection are discarded with it. (Up to SDK 7.27.7 every variable of every
slave was published each cycle: a slave that stopped answering repeated its
last values, and one that never answered came out as zeros.)

### Exceptions

An exception response is logged as an error with its code and name and the
block it answered (`cur_map_`), and the cycle moves on at once. Over TCP the
frame ends at the exception code; RTU adds its CRC. (Up to SDK 7.27.7 the TCP
master waited after an exception for a CRC that TCP does not carry, so each
exception cost `timeout_response` and held the poll of every slave of the
connection.)

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
