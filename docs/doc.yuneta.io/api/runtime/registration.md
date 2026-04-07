# GClass Registration

Every GClass must be registered before it can be instantiated.
Each `register_c_*()` function follows the same signature and pattern.

All built-in GClasses are registered at once by calling
[`yunetas_register_c_core()`](entry_point.md#yunetas_register_c_core).

**Source:** `kernel/c/root-linux/src/c_*.h`

---

## Signature

```C
int register_c_<name>(void);
```

**Parameters** — none.

**Returns** — `0` on success.

---

## Complete list

| Function | GClass | Description |
|----------|--------|-------------|
| `register_c_auth_bff()` | `C_AUTH_BFF` | Auth Backend-For-Frontend (OAuth 2 PKCE). |
| `register_c_authz()` | `C_AUTHZ` | Authorization manager. |
| `register_c_channel()` | `C_CHANNEL` | Logical communication channel. |
| `register_c_counter()` | `C_COUNTER` | Event counter. |
| `register_c_fs()` | `C_FS` | File-system watcher. |
| `register_c_gss_udp_s()` | `C_GSS_UDP_S` | GSS UDP server. |
| `register_c_ievent_cli()` | `C_IEVENT_CLI` | Inter-event client. |
| `register_c_ievent_srv()` | `C_IEVENT_SRV` | Inter-event server. |
| `register_c_iogate()` | `C_IOGATE` | I/O gate (protocol mux). |
| `register_c_mqiogate()` | `C_MQIOGATE` | Message-queue I/O gate. |
| `register_c_node()` | `C_NODE` | Node management. |
| `register_c_ota()` | `C_OTA` | Over-the-air updates. |
| `register_c_prot_http_cl()` | `C_PROT_HTTP_CL` | HTTP client protocol. |
| `register_c_prot_http_sr()` | `C_PROT_HTTP_SR` | HTTP server protocol. |
| `register_c_prot_raw()` | `C_PROT_RAW` | Raw (pass-through) protocol. |
| `register_c_prot_tcp4h()` | `C_PROT_TCP4H` | TCP with 4-byte header protocol. |
| `register_c_pty()` | `C_PTY` | Pseudo-terminal. |
| `register_c_qiogate()` | `C_QIOGATE` | Queue I/O gate. |
| `register_c_resource2()` | `C_RESOURCE2` | Resource management. |
| `register_c_task()` | `C_TASK` | Task management. |
| `register_c_task_authenticate()` | `C_TASK_AUTHENTICATE` | Authentication task. |
| `register_c_tcp()` | `C_TCP` | TCP client. |
| `register_c_tcp_s()` | `C_TCP_S` | TCP server. |
| `register_c_timer()` | `C_TIMER` | High-level timer. |
| `register_c_timer0()` | `C_TIMER0` | Low-level timer (io_uring). |
| `register_c_tranger()` | `C_TRANGER` | Timeranger wrapper. |
| `register_c_treedb()` | `C_TREEDB` | TreeDB wrapper. |
| `register_c_uart()` | `C_UART` | UART / serial port. |
| `register_c_udp()` | `C_UDP` | UDP client. |
| `register_c_udp_s()` | `C_UDP_S` | UDP server. |
| `register_c_websocket()` | `C_WEBSOCKET` | WebSocket protocol. |
| `register_c_yuno()` | `C_YUNO` | Yuno (main daemon grandmother). |

---

## Anchors for individual functions

(register_c_auth_bff)=
(register_c_authz)=
(register_c_channel)=
(register_c_counter)=
(register_c_fs)=
(register_c_gss_udp_s)=
(register_c_ievent_cli)=
(register_c_ievent_srv)=
(register_c_iogate)=
(register_c_mqiogate)=
(register_c_node)=
(register_c_ota)=
(register_c_prot_http_cl)=
(register_c_prot_http_sr)=
(register_c_prot_raw)=
(register_c_prot_tcp4h)=
(register_c_pty)=
(register_c_qiogate)=
(register_c_resource2)=
(register_c_task)=
(register_c_task_authenticate)=
(register_c_tcp)=
(register_c_tcp_s)=
(register_c_tranger)=
(register_c_treedb)=
(register_c_uart)=
(register_c_udp)=
(register_c_udp_s)=
(register_c_websocket)=
