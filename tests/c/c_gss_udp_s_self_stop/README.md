# c_gss_udp_s_self_stop test

Tests `C_GSS_UDP_S` (`kernel/c/root-linux/src/c_gss_udp_s.c`) when its
`C_UDP_S` stops by itself.

A driver gclass (`C_TEST_GSS_SELF_STOP`) creates a `C_GSS_UDP_S` child on
`udp://127.0.0.1:34296` (`timeout_base` 1 s, `disable_end_of_frame`, so each
datagram comes in the gbuffer `C_UDP_S` read it into) and a plain UDP socket,
the peer:

- At its create, a second `C_GSS_UDP_S` with `timeout_base` 0: refused with a
  warning (*"timeout_base <= 0 would arm no timer ..."*), and the attribute
  reads the default `5000`.
0. The `C_UDP_S`'s `rx_buffer_size` goes down to `0` and the peer sends
   `one`. The host keeps that gbuffer and, in the same stack, sends `early`
   (a send in flight) and posts itself an event. The next read needs a new
   gbuffer, of 0 bytes: it cannot start, and `C_UDP_S` stops by itself, in
   `ST_WAIT_STOPPED` until `early` completes. The posted event comes first:
   the host sends `wait` to a `C_UDP_S` still stopping, refused with ONE
   warning (*"EV_SEND_MESSAGE while the UDP server is stopped, dropped"*).
   Then `EV_STOPPED`: *"UDP server stopped by itself, it is started again
   after a backoff"*.
1. The host sends `lost`: refused, no second warning.
2. After `timeout_base` the `C_UDP_S` is started again (*"UDP server started
   again"*); the peer sends `two`, and the host must get it.
3. The host sends `back`; the peer must get `early` and `back`, nothing else.
4. The `C_UDP_S` stops by itself again (`three` kept), within a minute of its
   restart: the backoff doubles to 2 s. It must still be down 1.3 s after
   that stop, and up again at 2.3 s.
5. Then it is stopped from outside (`gobj_stop()` of the `C_UDP_S`): *"UDP
   server stopped from outside, it is not started again"*, a send meanwhile
   refused with one warning, and 1.2 s later it is still down. At its stop
   the `C_GSS_UDP_S` says the sends refused since (*"UDP server stops with
   sends refused while it was stopped"*).

Up to 7.25.20 `C_GSS_UDP_S` took that `EV_STOPPED` with no action: `lost` and
`back` each logged *"Event NOT DEFINED in state"* (from `C_UDP_S` in
`ST_STOPPED`), and the host never got `two`. The first fix of the self-stop
still sent `wait` to the `C_UDP_S` in `ST_WAIT_STOPPED` (*"Event NOT DEFINED
in state"*), restarted it at every tick with no backoff, restarted a
`C_UDP_S` stopped from outside, and took a `timeout_base` of 0 silently (no
timer: no peer forgotten, no restart).

## Run

```bash
ctest -R test_c_gss_udp_s_self_stop --output-on-failure --test-dir build
```
