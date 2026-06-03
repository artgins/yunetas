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

The master runs over [`C_TCP`](#gclass-c-tcp) (`"modbus_protocol": "TCP"`). The slave/register
map is JSON config: a `slaves` array, each with its register definitions
(`type`, address, format, multiplier). The master polls them on a timer.

**Commands:** `dump_data` (current register snapshot), `set-poll-timeout`.

**Trace levels:** `messages`, `traffic` (raw bytes), `polling`, `decode`, `send`.
