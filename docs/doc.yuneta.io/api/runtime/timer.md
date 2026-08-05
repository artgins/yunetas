# Timer

High-level and low-level timer helpers for GObjects.

**Source:** `kernel/c/root-linux/src/c_timer.h`,
`kernel/c/root-linux/src/c_timer0.h`

## C_TIMER — high-level timer

`C_TIMER` is the recommended timer GClass. It manages `gobj_start()` /
`gobj_stop()` internally — use the helper functions below instead of
calling those directly.

The two calls are the **whole contract**: `set_timeout..()` arms,
`clear_timeout()` disarms, and a one-shot is spent once it fires. Whether the
gobj is running is internal, so a caller never pairs a `set_timeout()` with a
`gobj_start()`, and never stops the timer on the way out.

These helpers are the rare case of a GClass exporting PUBLIC C functions, an
escape from the [GClass interface](../../../../yunos/c/yuno_agent/GOBJ.md). They are therefore **sugar
over the `msec` attribute** and hold no behaviour of their own — the arming
lives in `mt_writing`, so

```C
gobj_write_integer_attr(timer, "msec", 1000);
```

leaves the timer exactly as `set_timeout(timer, 1000)` does. Write `periodic`
**before** `msec`: the `msec` write is what arms.

The JS port (`@yuneta/gobj-js`, `C_TIMER`) carries the same contract, function
for function.

---

(register_c_timer)=
## `register_c_timer()`

Registers the `C_TIMER` GClass.

```C
int register_c_timer(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

`0` on success.

---

(set_timeout)=
## `set_timeout()`

Arms a **one-shot** timer. Starts the gobj if it is not already running.

```C
void set_timeout(hgobj gobj, json_int_t msec);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Timer gobj instance (must be a `C_TIMER`). |
| `msec` | `json_int_t` | Timeout in milliseconds. |

**Returns**

This function does not return a value.

---

(set_timeout_periodic)=
## `set_timeout_periodic()`

Arms a **periodic** (recurring) timer. Starts the gobj if it is not
already running.

```C
void set_timeout_periodic(hgobj gobj, json_int_t msec);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Timer gobj instance (must be a `C_TIMER`). |
| `msec` | `json_int_t` | Interval in milliseconds. |

**Returns**

This function does not return a value.

---

(clear_timeout)=
## `clear_timeout()`

Disarms and stops the timer.

```C
void clear_timeout(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Timer gobj instance (must be a `C_TIMER`). |

**Returns**

This function does not return a value.

---

## C_TIMER0 — low-level timer (io_uring)

`C_TIMER0` is a low-level timer that uses `io_uring` directly.
Each instance opens a file descriptor. **Prefer `C_TIMER`** unless you
need the lower-level control.

---

(register_c_timer0)=
## [`register_c_timer0()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_timer0.c#L309)

Registers the `C_TIMER0` GClass.

```C
int register_c_timer0(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

`0` on success.

---

(set_timeout0)=
## [`set_timeout0()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_timer0.c#L327)

Arms a **one-shot** low-level timer.

```C
void set_timeout0(hgobj gobj, json_int_t msec);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Timer gobj instance (must be a `C_TIMER0`). |
| `msec` | `json_int_t` | Timeout in milliseconds. |

**Returns**

This function does not return a value.

---

(set_timeout_periodic0)=
## [`set_timeout_periodic0()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_timer0.c#L371)

Arms a **periodic** low-level timer.

```C
void set_timeout_periodic0(hgobj gobj, json_int_t msec);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Timer gobj instance (must be a `C_TIMER0`). |
| `msec` | `json_int_t` | Interval in milliseconds. |

**Returns**

This function does not return a value.

---

(clear_timeout0)=
## [`clear_timeout0()`](https://github.com/artgins/yunetas/blob/7.9.9/kernel/c/root-linux/src/c_timer0.c#L415)

Disarms and stops the low-level timer.

```C
void clear_timeout0(hgobj gobj);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Timer gobj instance (must be a `C_TIMER0`). |

**Returns**

This function does not return a value.
