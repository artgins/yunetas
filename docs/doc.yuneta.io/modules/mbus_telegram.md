(module-mbus-telegram)=
# M-Bus telegrams

The telegram decoder of the [M-Bus module](#module-mbus): it finds the frames of
EN 13757-2 in a stream of bytes, builds the requests of a master, and decodes
the variable data structure of EN 13757-3 (the answer of a meter) to json.
`C_PROT_MBUS_M` is built on it. A gate that reads M-Bus some other way, or a
tool that decodes captured telegrams, can use it directly.

Source code:

- [`mbus_telegram.h`](https://github.com/artgins/yunetas/blob/7.29.3/modules/c/mbus/src/mbus_telegram.h)
- [`mbus_telegram.c`](https://github.com/artgins/yunetas/blob/7.29.3/modules/c/mbus/src/mbus_telegram.c)

**Nothing here logs, and nothing keeps state.** A function that fails returns
`-1`, `0` or `NULL`, and writes the reason into the `why` buffer it is given.
The caller logs that reason, because only the caller knows which peer sent the
bytes. A meter that answers garbage is a warning about that meter, not an
error of the decoder.

**How it was checked.** The decoder was compared, record by record, with
libmbus's decoding of the 78 real telegrams in libmbus's test collection. 893
of the 909 records are equal. The other 16 differ on purpose:

- a BCD with a digit in error is `null` here, where libmbus makes up a number;
- a "date of" VIFE (`6F`, the date of a maximum) is a date here, where libmbus
  reads a temperature of 41 million °C;
- a date that was never set is `null`;
- a date with a field out of its range (an hour 31, a year 127) is `null`.
  This one came after the comparison: since then, storage 510 of the
  Landis+Gyr T230 (a type F date with every year bit set, a slot never
  written) is `null` here and `2027-01-01T00:00` in libmbus;
- the fixed data structure (`CI 73`) is not decoded.

The module's test (`tests/c/mbus_telegram`) keeps six of those telegrams.

---

(mbus_parse_frame)=
## `mbus_parse_frame()`

`mbus_parse_frame()` finds the frame that starts at `bf[0]`: an `E5`, a short
frame (`10 C A CS 16`), a control frame or a long frame
(`68 L L 68 C A CI data CS 16`).

```C
int mbus_parse_frame(
    const uint8_t *bf,
    size_t len,
    mbus_frame_t *frame,
    char *why,
    size_t why_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf`, `len` | `const uint8_t *`, `size_t` | The bytes received so far. |
| `frame` | `mbus_frame_t *` | Filled when a frame is found: `type`, `c`, `a`, `ci`, and `data`/`data_len`, the bytes after CI. `data` points into `bf`. |
| `why`, `why_size` | `char *`, `size_t` | Where the reason of a refusal is written. |

**Returns**

- The length of the frame (`> 0`).
- `0` when `bf` holds the start of a frame and more bytes are needed.
- `-1` when `bf` does not start with a valid frame: a bad start byte, the two
  lengths differ, a bad checksum or stop byte. The link layer cannot
  resynchronise inside a broken frame, so the caller drops what it holds.

Bytes after the frame are not looked at: the caller decides whether they are
the next frame or noise.

**Example**

```C
mbus_frame_t frame;
char why[256];
int r = mbus_parse_frame(priv->rx, priv->rx_len, &frame, why, sizeof(why));
if(r == 0) {
    return 0;   // wait for more bytes
}
if(r < 0) {
    gobj_log_warning(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_PROTOCOL,
        "msg",          "%s", "M-Bus bad frame",
        "why",          "%s", why,
        NULL
    );
    priv->rx_len = 0;
    return -1;
}
// frame.type is MBUS_FRAME_ACK, _SHORT, _CONTROL or _LONG
```

---

(mbus_build_short_frame)=
## `mbus_build_short_frame()`

`mbus_build_short_frame()` builds a short frame, the shape of every request of
a master that carries no data: `SND_NKE`, `REQ_UD2`.

```C
size_t mbus_build_short_frame(uint8_t *bf, size_t size, uint8_t c, uint8_t a);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf`, `size` | `uint8_t *`, `size_t` | Where the frame is written. |
| `c` | `uint8_t` | The control field: `MBUS_C_SND_NKE`, or `MBUS_C_REQ_UD2` with or without `MBUS_C_FCB`. |
| `a` | `uint8_t` | The address: 1-250, or `MBUS_ADDRESS_SELECTED` (`FD`) for the slave selected by its secondary address. |

**Returns**

5, the length written, or `0` when `size` is less than that.

**Example**

```C
uint8_t bf[MBUS_FRAME_MAX_LENGTH];
size_t n = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_REQ_UD2|MBUS_C_FCB, 1);
// 10 7B 01 7C 16
```

---

(mbus_build_long_frame)=
## `mbus_build_long_frame()`

`mbus_build_long_frame()` builds a long frame: the `SND_UD` of a master (a
selection, a write), or the `RSP_UD` of a meter in a test.

```C
size_t mbus_build_long_frame(
    uint8_t *bf,
    size_t size,
    uint8_t c,
    uint8_t a,
    uint8_t ci,
    const uint8_t *data,
    size_t data_len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf`, `size` | `uint8_t *`, `size_t` | Where the frame is written: `data_len + 9` bytes. |
| `c`, `a`, `ci` | `uint8_t` | Control, address and control information fields. |
| `data`, `data_len` | `const uint8_t *`, `size_t` | The bytes after CI, 252 at most. |

**Returns**

The length written, or `0` when `data_len` is more than a frame carries or
`size` is too small.

**Example**

```C
uint8_t sel[MBUS_SECONDARY_ADDRESS_SIZE];
mbus_build_secondary_address(sel, "12345678", "KAM", -1, -1, why, sizeof(why));
size_t n = mbus_build_long_frame(bf, sizeof(bf),
    MBUS_C_SND_UD, MBUS_ADDRESS_SELECTED, MBUS_CI_SELECT_SECONDARY,
    sel, sizeof(sel)
);
// 68 0B 0B 68 53 FD 52 78 56 34 12 2D 2C FF FF 0D 16
```

---

(mbus_build_secondary_address)=
## `mbus_build_secondary_address()`

`mbus_build_secondary_address()` encodes the 8 bytes that select a meter by
its secondary address: identification (4 BCD bytes, least significant first),
manufacturer, version, medium.

```C
int mbus_build_secondary_address(
    uint8_t out[MBUS_SECONDARY_ADDRESS_SIZE],
    const char *id,
    const char *manufacturer,
    int version,
    int medium,
    char *why,
    size_t why_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `out` | `uint8_t[8]` | Where the selection is written. |
| `id` | `const char *` | 8 digits. An `F` in a digit is a wildcard (`"1234FFFF"`), as a search of the bus uses them. |
| `manufacturer` | `const char *` | 3 letters (`"KAM"`), or `NULL`/`""` for any. |
| `version` | `int` | 0-255, or `-1` for any. |
| `medium` | `int` | 0-255, or `-1` for any. [`mbus_medium_code()`](#mbus_medium_code) gives it from a name. |
| `why`, `why_size` | `char *`, `size_t` | The reason of a refusal. |

**Returns**

`0`, or `-1` when a field is not valid.

**Example**

```C
uint8_t sel[MBUS_SECONDARY_ADDRESS_SIZE];
char why[256];
if(mbus_build_secondary_address(sel, "1234ffff", NULL, -1, mbus_medium_code("water"),
        why, sizeof(why)) < 0) {
    // why: "secondary address id must be digits or F: '...'"
}
// FF FF 34 12 FF FF FF 07
```

---

(mbus_decode_user_data)=
## `mbus_decode_user_data()`

`mbus_decode_user_data()` decodes the user data of a meter's long frame
(`RSP_UD`) to json.

```C
json_t *mbus_decode_user_data(
    const mbus_frame_t *frame,
    BOOL *more_follows,
    char *why,
    size_t why_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `frame` | `const mbus_frame_t *` | A long frame found by [`mbus_parse_frame()`](#mbus_parse_frame). |
| `more_follows` | `BOOL *` | Set when the telegram ends with `DIF 1F`: the meter has more records, asked for with the FCB toggled. May be `NULL`. |
| `why`, `why_size` | `char *`, `size_t` | The reason of a refusal. |

**Returns**

The json, yours to decref, or `NULL` with the reason in `why`.

| CI | Decoded to |
|---|---|
| `72` | `header` (12 bytes: `id`, `manufacturer`, `version`, `medium`, `medium_code`, `access_no`, `status`) and `records` |
| `7A` | `header` (`access_no`, `status`) and `records` |
| `78` | `records` only |
| `70` | `{"application_error": <code>}` |
| `71` | `{"alarm_status": "<hex>"}` |

It refuses the fixed data structure (`CI 73`/`77`), the other CIs, records
encrypted (security modes 5, 7, 8, 9, 10 and 13 of the configuration field),
and a record that does not fit in the telegram.

**The id of a record** says what the record is, and stays the same from one
reading to the next:

```
<quantity>[:max|:min|:error][@s<n>][@t<n>][@u<n>][~<vife>...][#<n>]
```

| Part | From | Example |
|---|---|---|
| quantity | the VIF (and a VIFE that changes it) | `volume`, `energy`, `flow_temperature`, `volume_neg` |
| `:max`, `:min`, `:error` | the function of the DIF (instantaneous has none) | `power:max` |
| `@s<n>` | the storage number, when not 0 | `volume@s1`, often the value at the due date |
| `@t<n>`, `@u<n>` | the tariff, the subunit, when not 0 | `energy@t2`, `volume@u1` |
| `~<vife>` | a VIFE not interpreted, in hex | `volume~22`, `flow_temperature:max@t1~6f` (the date of the maximum) |
| `#<n>` | the n-th record with the same id in the reading | `power#2` |

**Values come in one unit per quantity**, already scaled:

| Quantity | Unit | Also from |
|---|---|---|
| `energy` | `Wh` | J, MWh, GJ |
| `power` | `W` | J/h, MW, GJ/h |
| `volume` | `m3` | |
| `volume_flow` | `m3/h` | m3/min, m3/s |
| `mass`, `mass_flow` | `kg`, `kg/h` | t |
| `flow_temperature`, `return_temperature`, `external_temperature` | `C` | °F |
| `temperature_difference` | `K` | °F |
| `pressure` | `bar` | |
| `on_time`, `operating_time`, `averaging_duration`, `storage_interval`, `tariff_period`, `since_last_cumulation`, `battery_operating_time`, ... | `s` | min, h, days |
| `storage_interval_months`, `tariff_period_months`, `since_last_cumulation_months`, `battery_operating_time_months` | `month` | years |
| `voltage`, `current` | `V`, `A` | |

So a consumer that reads `energy` never has to ask whether the meter counted it
in GJ. A month or a year is no fixed number of seconds, so a duration the meter
counts in months or years is another quantity, with `_months` in its name (a
year is 12 months):

```json
{"storage_interval":        {"value": 900, "unit": "s"},
 "storage_interval_months": {"value": 24,  "unit": "month"}}
```

The one quantity with no fixed unit is `custom`, a plain text VIF: its unit is
the text the meter sends.

Dates come as text: `"2026-10-23"` (type G), `"2026-10-23T14:05"` (type F),
`"2026-10-23T14:05:30"` (type I) or `"14:05:30"` (type J). Identity numbers
come as text too (`fabrication_number`). These have `"value": null`: a real
that is NaN or `+-Inf` (json has no number for them; the other records of the
telegram are decoded), a BCD with a digit in error, a date marked invalid,
never set, or with a field out of its range (year 0-99, month 1-12, day 1-31,
hour 0-23, minute and second 0-59), and a record that carries a record-error
VIFE (it also has `"error": "no data available"`, or whatever the code says). Manufacturer data
(after `DIF 0F`) comes as `mfr_data` in hex.

**Example**

```C
BOOL more;
char why[256];
json_t *jn = mbus_decode_user_data(&frame, &more, why, sizeof(why));
if(!jn) {
    // why: "records encrypted (security mode 5): not decoded"
    return -1;
}
json_t *jn_volume = json_object_get(json_object_get(jn, "records"), "volume");
double m3 = json_number_value(json_object_get(jn_volume, "value"));
JSON_DECREF(jn)
```

A record id is a **key**: read it with `json_object_get()`, never as a kw path.

---

(mbus_medium_name)=
## `mbus_medium_name()`

`mbus_medium_name()` gives the name of a medium code, the one a header carries
as `medium`.

```C
const char *mbus_medium_name(int medium);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `medium` | `int` | The medium byte of a header or of a secondary address. |

**Returns**

A static name, or `NULL` for a code that has none:

| Code | Name | Code | Name |
|---|---|---|---|
| `01` | `oil` | `07` | `water` |
| `02` | `electricity` | `08` | `heat_cost_allocator` |
| `03` | `gas` | `0C` | `heat_inlet` |
| `04` | `heat` | `15` | `hot_water` |
| `06` | `warm_water` | `16` | `cold_water` |

and the rest of the codes in EN 13757-3 (`steam`, `compressed_air`,
`cooling_outlet`, `pressure`, `smoke_detector`, `valve`, `waste_water`, ...).

**Example**

```C
const char *name = mbus_medium_name(0x07);  // "water"
```

---

(mbus_medium_code)=
## `mbus_medium_code()`

`mbus_medium_code()` gives the code of a medium name: the reverse of
[`mbus_medium_name()`](#mbus_medium_name).

```C
int mbus_medium_code(const char *name);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | A name of the table above. |

**Returns**

The code, or `-1` for a name it does not know (or an empty one).

**Example**

```C
int medium = mbus_medium_code("gas");   // 3
```
