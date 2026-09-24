# Yuno

Functions for managing the yuno (daemon process), its event loop,
and IP-based access control.

**Source:** `kernel/c/root-linux/src/c_yuno.h`

---

(register_c_yuno)=
## `register_c_yuno()`

Registers the `C_YUNO` GClass — the main grandmother GClass for every
yuno daemon.

```C
int register_c_yuno(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

`0` on success.

---

(yuno_event_loop)=
## [`yuno_event_loop()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/c_yuno.c#L5898)

Returns the yuno's event loop handle. The return type is `void *` to
avoid exposing the `yev_loop.h` header to callers.

```C
void *yuno_event_loop(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Pointer to the `yev_loop` instance (cast to `void *`).

---

(yuno_event_destroy)=
## [`yuno_event_destroy()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/c_yuno.c#L5906)

Destroys and frees the yuno event loop.

```C
void yuno_event_destroy(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

---

(set_yuno_must_die)=
## [`set_yuno_must_die()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/c_yuno.c#L5917)

Orders the yuno to exit gracefully. Logs an exit message, sets the exit
code to `0`, flushes logs, and calls
[`yuno_shutdown()`](runtime_entry_point.md#yuno_shutdown).

```C
void set_yuno_must_die(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

---

(is_ip_allowed)=
## [`is_ip_allowed()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/c_yuno.c#L5936)

Checks whether an IP address is in the allowed-IPs list.
The lookup key is the ip of the peername, without its port:

| `peername` | key |
|---|---|
| `"192.168.1.1:8080"` | `192.168.1.1` |
| `"[2001:db8::1]:443"` | `2001:db8::1` |
| `"[::ffff:192.168.1.1]:80"` (an ipv4 on a dual-stack socket) | `192.168.1.1` |
| `"192.168.1.1"`, `"2001:db8::1"` (no port) | the same |

Up to 7.25.4 the port was cut at the first `:`, so no list could name an
ipv6 peer.

```C
BOOL is_ip_allowed(const char *peername);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `peername` | `const char *` | IP address or `IP:port` string. |

**Returns**

`TRUE` if the IP is allowed, `FALSE` otherwise.

:::{note}
Only the **read** half of the allowed/denied lists is public C API, because
`c_tcp_s` asks it once per accepted connection and `c_authz` once per login.
Since 7.25.5 `c_tcp_s` asks both lists at accept (see
[IP lists at accept](#tcp_s_ip_lists)).
Writing goes through the GClass interface: the `allowed_ips` / `denied_ips`
attributes (`SDF_PERSIST`) and the `add-allowed-ip`, `remove-allowed-ip`,
`add-denied-ip`, `remove-denied-ip` commands.
:::

---

(is_ip_denied)=
## [`is_ip_denied()`](https://github.com/artgins/yunetas/blob/7.25.4/kernel/c/root-linux/src/c_yuno.c#L5979)

Checks whether an IP address is in the denied-IPs list, with the same
lookup key as [`is_ip_allowed()`](#is_ip_allowed).
Denied IPs take precedence over allowed IPs.

```C
json_t *denied_ips = gobj_read_json_attr(gobj_yuno(), "denied_ips");
json_object_set_new(denied_ips, "2001:db8::1", json_true());
is_ip_denied("[2001:db8::1]:443");  // TRUE
is_ip_denied("[2001:db8::2]:443");  // FALSE
```

```C
BOOL is_ip_denied(const char *peername);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `peername` | `const char *` | IP address or `IP:port` string. |

**Returns**

`TRUE` if the IP is denied, `FALSE` otherwise.

---
