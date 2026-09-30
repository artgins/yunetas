# c_gss_udp_s_self_stop test

Tests `C_GSS_UDP_S` (`kernel/c/root-linux/src/c_gss_udp_s.c`) when its
`C_UDP_S` stops by itself.

A driver gclass (`C_TEST_GSS_SELF_STOP`) creates a `C_GSS_UDP_S` child on
`udp://127.0.0.1:34296` (`timeout_base` 1 s, `disable_end_of_frame`, so each
datagram comes in the gbuffer `C_UDP_S` read it into) and a plain UDP socket,
the peer:

0. The `C_UDP_S`'s `rx_buffer_size` goes down to `0` and the peer sends
   `one`. The host keeps that gbuffer, so the next read needs a new one, of 0
   bytes: it cannot start, and `C_UDP_S` stops by itself (`EV_STOPPED`).
   `C_GSS_UDP_S` must say it once (*"UDP server stopped by itself, it is
   started again at the next timeout_base"*).
1. The host sends `lost` to the peer: refused, ONE warning (*"EV_SEND_MESSAGE
   while the UDP server is stopped, dropped"*).
2. At the next `timeout_base` the `C_UDP_S` is started again (*"UDP server
   started again"*); the peer sends `two`, and the host must get it.
3. The host sends `back`, and the peer must get it (and not `lost`).

Up to 7.25.20 `C_GSS_UDP_S` took that `EV_STOPPED` with no action: `lost` and
`back` each logged *"Event NOT DEFINED in state"* (from `C_UDP_S` in
`ST_STOPPED`), and the host never got `two`.

## Run

```bash
ctest -R test_c_gss_udp_s_self_stop --output-on-failure --test-dir build
```
