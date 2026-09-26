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
## [`yuno_event_loop()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/root-linux/src/c_yuno.c#L5997)

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
## [`yuno_event_destroy()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/root-linux/src/c_yuno.c#L6005)

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
## [`set_yuno_must_die()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/root-linux/src/c_yuno.c#L6016)

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
## [`is_ip_allowed()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/root-linux/src/c_yuno.c#L6264)

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

(yuno-ip-list-entries)=
### The form of an entry

A peer is looked up by the text that the kernel gives for its address, so an
entry must be in the same form. Since 7.25.5 the `add-` and `remove-`
commands convert what you type to that form, and refuse what is not an ip:

| You type | Stored as |
|---|---|
| `2001:DB8::1`, `2001:db8:0:0:0:0:0:1` | `2001:db8::1` (lowercase, compressed) |
| `::ffff:203.0.113.7` | `203.0.113.7` (an ipv4-mapped ipv6 is its ipv4) |
| `fe80::1%eth0`, `fe80::1%2` | `fe80::1%2` (a link-local address with its interface index) |
| `fe80::1` | `fe80::1` in `denied_ips` only: it denies the address on every interface |
| `[2001:db8::1]`, `203.0.113.7:443`, `localhost`, `10.0.0.1%eth0` | refused, with the cause |

```bash
ycommand -c 'command-yuno id=<id> service=__yuno__ command=add-denied-ip ip=2001:DB8::1 denied=1'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=add-allowed-ip ip=fe80::10%eth0 allowed=1'
ycommand -c 'command-yuno id=<id> service=__yuno__ command=remove-denied-ip ip=2001:db8:0:0:0:0:0:1'
```

A link-local address (`fe80::/10`, `ff02::/16`) is the same text on every
link, and on another link it is another host. So `allowed_ips` requires its
interface, and matches it exactly: `fe80::10%2` does not let in `fe80::10`
that arrives on interface 3. `denied_ips` accepts it without the interface,
and then denies the address on every interface. A deny broader than asked is
the safe side.

The answers say what was done. An `add-` that converted the text names both
forms; text that is not an ip answers -1 with the cause; a `remove-` takes any
form of the same ip, and answers -1 when the ip is not in the list (up to
7.25.4 it answered success):

```text
add-denied-ip ip=2001:DB8::1 denied=1    ->  0: yuneta_agent^agent: '2001:DB8::1' stored as '2001:db8::1'
add-denied-ip ip=203.0.113.7:443 denied=1 -> -1: yuneta_agent^agent: ip '203.0.113.7:443' is not a numeric ipv4 or ipv6 address (no host name, port or brackets)
add-allowed-ip ip=fe80::10 allowed=1      -> -1: yuneta_agent^agent: ip 'fe80::10' is link-local: name its interface, like fe80::10%eth0 or fe80::10%2
remove-denied-ip ip=2001:db8::9           -> -1: yuneta_agent^agent: ip '2001:db8::9' is not in denied_ips
```

Up to 7.25.4 the commands stored the text as typed and answered success,
so an entry like `2001:DB8::1` was shown by `list-denied-ips` and never
matched a peer.

**Upgrade.** When the yuno starts, it rewrites the entries that an older
version stored as typed. An entry that names an ip in another form is
renamed, with a warning (*"ip list entry renamed to the form a peer is looked
up by"*). An entry that is not an ip (`203.0.113.7:443`, `[2001:db8::1]`, a
host name, or a link-local address without its interface in `allowed_ips`)
is removed, with a warning (*"ip list entry dropped, it never matched a
peer"*) that names it. Add it again in a valid form. Two entries of one ip
become one: in `denied_ips` the deny wins, in `allowed_ips` the refusal wins.
The lists are saved once, rewritten.

---

(is_ip_denied)=
## [`is_ip_denied()`](https://github.com/artgins/yunetas/blob/7.25.7/kernel/c/root-linux/src/c_yuno.c#L6307)

Checks whether an IP address is in the denied-IPs list, with the same
lookup key as [`is_ip_allowed()`](#is_ip_allowed). A link-local peer
(`fe80::1%2`) is also denied by the entry without its interface (`fe80::1`),
see [The form of an entry](#yuno-ip-list-entries).
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
