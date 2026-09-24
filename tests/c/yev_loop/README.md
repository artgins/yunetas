# yev_loop test

Integration tests for the io_uring-based event loop: timers, TCP client/server echo, TLS client/server echo (both OpenSSL and mbedTLS), event delivery ordering and clean shutdown.

`yev_events/test_yevent_sq_full` fills the submission queue with NOPs that are not submitted. A timer starts and stops, and the loop stops, with the queue full: the loop flushes the queue and asks again. Then the kernel is made to take nothing (the ring fd is made invalid for the call, so `io_uring_enter()` answers `EBADF`): the submission is kept, the start answers `0`, and the timer fires at the next cycle; a timer stopped while its start is still kept gets the callback `STOPPED` with `-ECANCELED`; a stop of a second loop is kept and stops it.

`yev_events/test_yevent_sq_retry` makes `io_uring_submit()` fail on demand (linked with `-Wl,--wrap=io_uring_submit`). A start whose submit fails while the queue has room leaves its entry in the queue: the loop submits it again at the next cycle, and the timer fires. A kept start whose hand-over submit fails fires too. A stop of a timer whose read is still in the queue takes the read back: a new timer that gets the same fd number fires, and the stopped one gets `STOPPED` with `-ECANCELED`. When the kernel takes nothing for 100 cycles, the loop logs one ERROR, and one INFO when the kernel takes the submissions again.

`yev_events/test_yevent_sq_nomem` limits the largest block to 100000 bytes, so the kept list cannot grow past 1024 entries. The 1025th write start answers `-1` with *"No memory to keep a submission: event NOT started"* (it aborted the process before), the 1024 kept writes complete, and the memory tracking is whole at the end.

`yev_events/test_yevent_udp_zerocopy` sends UDP datagrams with zero-copy. A zero-copy send gives two completions: the result (`IORING_CQE_F_MORE`) and a notification (`IORING_CQE_F_NOTIF`). The test checks that the callback is called once for each send, that an event destroyed in its callback stays alive until its notification (the test holds a reference to the gbuffer of the event and reads its refcount), that an event sent again before its notification gets only its own completions, and that a failed send (`-EMSGSIZE`) is `STOPPED` with no warning. In 7.25.4 the event was freed at the first completion and the notification read the freed event.

`yev_events/test_yevent_udp_ipv6` uses IPv6 peers on `[::1]`. A UDP listener on `udp://[::1]:0` keeps the IPv6 address. A recvmsg event receives a datagram from an IPv6 client, the peer address goes into the gbuffer with its length (`gbuffer_setaddr()`), and a sendmsg event sends the gbuffer back to it, as `C_UDP_S` does. A TCP connect to `tcp://[::1]` connects. In 7.25.4 the addresses were a `struct sockaddr` (16 bytes): the listener and the connect were refused, the peer was cut to 16 bytes, and the kernel refused the reply (`-EINVAL`). Without IPv6 on the host the test is skipped.

`yev_events/test_yevent_stop_in_flight` stops a read, a write to a full socket and a recvmsg while the kernel has them. The event keeps its gbuffer until the completion of the cancel (the test holds a reference to the gbuffer and reads its refcount), and the callback gets the event `STOPPED`, `-ECANCELED` and without gbuffer. The read is started again with a new gbuffer and reads. `yev_set_gbuffer(NULL)` on a running read releases the gbuffer at the completion too. In 7.25.4 the stop released the gbuffer at once, while the kernel could still write into it.

## Run

```bash
ctest -R test_yev_loop --output-on-failure --test-dir build
```

Benchmarks live under `performance/c/perf_yev_ping_pong*`.
