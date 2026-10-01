# emailsender test

Tests of the `emailsender` yuno's gclasses (`C_EMAILSENDER`, `C_SMTP_SESSION`
and the MIME encoder, from `yunos/c/emailsender/src`, compiled into each test)
against a fake SMTP server.

## Topology

- **Fake SMTP server**: `C_FAKE_SMTP` over `__input_side__`
  (`C_IOGATE` → `C_TCP_S` + `C_CHANNEL` → `C_FAKE_SMTP` → `C_TCP`), plain SMTP
  with no TLS. It answers `AUTH` with the lines of its `auth_replies`, one per
  `AUTH`, the last one again when they run out, and logs each answer and each
  message delivered, so the list of expected logs of a test says what the
  server saw. With `die_on_delivery` the yuno ends a second after the first
  delivery. With `notify_service` it tells that service (`EV_FAKE_CLIENT_CONNECTED`)
  `notify_delay` ms after a client connects, and greets it `banner_delay` ms
  later: a test acts while the client is in its handshake. With `auth_min_gaps` an
  AUTH sooner than its gap after the previous one is an ERROR ("Fake smtp: AUTH
  too early").
  RCPT TO and the end of DATA are answered from `rcpt_replies` and
  `data_replies` the same way; only a 250 to the end of DATA is a delivery
  (anything else logs "Fake smtp: message refused"), and after a `421` the
  server closes the connection. `connection_plan` says what it does with each
  connection: `greet`, `drop` (closed at once), `garbage` (two malformed lines
  in one write) or `long_line` (a line longer than the client's reply buffer).
  `connect_min_gaps` / `connect_max_gaps` and `data_min_gaps` check the pacing
  of the connections and of the uploads like `auth_min_gaps` ("Fake smtp:
  connection too early" / "too late", "Fake smtp: DATA too early").
- **Driver**: `C_TEST_EMAILSENDER`, one `scenario` per test (see its header).
  A test that needs the fake server up before the emailsender connects lists it
  first, `autostart`/`autoplay`, under its own name (the emailsender starts and
  stops its `__input_side__` with its own play and pause).

`pause_in_flight`, `shutdown_in_flight`, `set_url_stash` and `no_recipients`
run with freed memory poisoned (`poison_alloc.c`, the technique of
`tests/c/c_controlcenter_scenarios`): every block freed is filled with `0x5A`
and held in a quarantine of 4096 blocks before it is given back, so a read
after a free reads the poison. Without it the queue opened again after a pause
loads the same message into the very block just freed, and a dangling
`q_msg_t` reads right. Against the code before the fixes each of the four
segfaults on every run. The quarantine is emptied, and closed, in `cleaning()`
and after `yuneta_entry_point()`, before the memory check: the blocks it holds
are freed memory, and a clean run reports none.

Each test also lists the messages it expects logged as ERROR (most expect
none): the list of expected logs says what was logged, not at which level, so
a second log handler collects every ERROR and the test compares them at its
end. A failure the SMTP server causes is a WARNING, not an ERROR. Every test whose
fake server is not listening when the emailsender plays (it is the
`__input_side__` the emailsender starts after its session, or one the driver
starts) sees one refused connection first: the WARNING "SMTP server failing"
is in its list, and the INFO "SMTP server works again" before "email sent".

A test whose yuno exits by itself (`LOG_OPT_EXIT_ZERO`, as `C_EMAILSENDER`
does on a refused login) never reaches its checks: an `atexit()` guard turns
that exit into a failure.

## Tests

| Test | Port | What it proves |
|------|------|----------------|
| `auth_transient` | 7821 | the server answers the first `AUTH` with `454 4.7.0 Temporary authentication failure` and the second with `235`: the message is delivered, no retry spent (the server never saw it). Up to 7.25.20 any answer but `235` was taken as rejected credentials, and the yuno exited with 0, not to be relaunched |
| `auth_rejected` | 7822 | a `C_SMTP_SESSION` of the driver's own gets `535 5.7.8 ...`: its `EV_ON_CLOSE` carries `auth_rejected: 535` and the reply text in `reply`, which up to 7.25.20 it did not carry |
| `set_email_user` | 7823 | the service starts with no credentials and a url where nobody listens (7829); `set-email-user` gives it the credentials and the url of the fake server, and the email is delivered THERE. Up to 7.25.20 the SMTP side was started before the url was written, and on the url its session was created with |
| `set_url_running` | 7824 | the service starts WITH credentials and the dead url, so its session runs; `set-url-from` gives it the url of the fake server (its answer says the url waits for the next start), the service is paused and played, and the email is delivered at the new url. Up to 7.25.20 the session kept the url it was created with until the yuno was restarted. Here the fake server is not the service's `__input_side__` (a pause stops that one): the driver starts it |
| `send_in_handshake` | 7825 | the email is sent while the SMTP session waits for the greeting: it waits for the handshake and is delivered. Up to 7.25.20 `C_SMTP_SESSION` took `EV_SEND_MESSAGE` only disconnected or idle: "Event NOT DEFINED", and since the emailsender retries a refused send at once, every retry was spent in the same instant and the email went to the failed queue |
| `auth_backoff` | 7826 | the server answers `454` to three AUTHs, then `235`; with `timeout_retry` 1 s the gaps between the AUTHs must be at least 1, 2 and 4 s, and the email, with `max_retries` 2, is delivered: a failed login spends no retry. Up to 7.25.20 every retry came after the transport's fixed 2 s ("Fake smtp: AUTH too early" at the fourth), and each failed login spent a retry |
| `pause_in_flight` | 7827 | the emailsender is paused while its email is in flight (the session in its handshake) and played in the next cycle of the loop, its C_TCP still closing: the email is delivered once, no retry spent. Up to 7.25.20 the pause freed the queued message while `qmsg_cur_email` kept pointing at it (the late `EV_ON_CLOSE` resolved freed memory: "email NOT sent, will retry"), and the play restarted a C_TCP still closing ("Initial wrong tcp state") |
| `shutdown_in_flight` | 7830 | the yuno is told to die with the email in flight: it stays queued and nothing is said about it. Up to 7.25.20 the `EV_ON_CLOSE` of the closing session resolved the freed message (a segfault here) |
| `set_url_stash` | 7831 | the email of the play waits in the SMTP session (dead url); `set-url-from` gives it the fake server, and a pause and a play send it once, at the new url. Up to 7.25.20 the waiting message survived the stop of the session and the owner kept a dangling pointer to its queued copy |
| `no_recipients` | 7832 | an `EV_SEND_EMAIL` whose `to` holds no address (`","`, no cc), then a good one: the first goes to the failed queue once (one ERROR), the second is delivered. Up to 7.25.20 the session answered the refusal with `EV_ON_MESSAGE` AND returned -1, and the emailsender resolved the message twice: its retries went in one instant, re-sent from inside the first answer, and the unwinding frames touched it after the failed queue had freed it (a segfault here) |
| `data_4xx_paced` | 7833 | the server answers the end of DATA with `451`, then `421` (and closes), then `250`; the uploads must come at least 1 s and 2 s apart (`timeout_retry` 1 s). Up to 7.25.20 a `4xx` there left the session up and the message was uploaded again at once ("Fake smtp: DATA too early"). It runs with no inactivity timeout, so the C_TCP arms its reconnection after the session's timer: the `EV_CONNECT` of the session must cancel it, or it fires in `ST_WAIT_CONNECTED` ("Event NOT DEFINED in state", twice) |
| `server_close_paced` | 7834 | the server closes the first connection at once, answers the second with two malformed lines in one write, closes the third at once and greets the fourth: the connections come at least 1, 2 and 4 s apart, the third within 3.5 s of the second (two aborts of one read double the delay once), and the email, with `max_retries` 2, is delivered: none of those failures is a retry of it. Up to 7.25.20 a close by the server reconnected after a fixed 2 s |
| `late_server` | 7835 | the server is down for the first 8.5 s; with `timeout_retry` 1 s the connection attempts come at 0, 1, 3, 7 and 15 s, so the session reaches it at least 4 s after it is up. Up to 7.25.20 a refused connection was retried every 2 s for ever (`timeout_retry_max` never reached the C_TCP). And it is said: the WARNING "SMTP server failing" at the first refusal with its cause ("cannot connect: Connection refused"), the ERROR of `timeout_failing_alarm` (5 s here) once, and the INFO "SMTP server works again" at the delivery; up to 7.25.20 nothing was logged at the default levels |
| `refill_paced` | 7836 | the server refuses the recipient of the first email (`550`, to the failed queue) and the session waits 3 s to reconnect; a second email 1.5 s later must wait for them. Up to 7.25.20 it connected at once ("Fake smtp: connection too early") |
| `auth_334` | 7839 | a `C_SMTP_SESSION` of the driver's own gets `334 ` to its `AUTH PLAIN`, answers it once with the same response, and gets `334 ` again: its `EV_ON_CLOSE` carries `auth_rejected: 334` (the emailsender stops on it, as on refused credentials). In the branch before this fix the first `334` was the refusal (and earlier a transient failure, logged in again for ever) |
| `long_line` | 7837 | the server greets the first connection with a line longer than the reply buffer of the session: one WARNING ("SMTP reply line too long"), no ERROR, and the email is delivered on the next connection. Up to 7.25.20 filling the istream logged "gbuf FULL" (and two gbuffer errors) as ERRORs with a stack |
| `url_log` | 7838 | the running emailsender is given a dead url with `set-url-from`, then sends one email: the session goes on with the url it runs on, and "email sent" must name that url. Up to 7.25.20 it named the new url, where nothing had been sent |
| `idle_close_no_reconnect` | 7842 | after the delivery the server ends the idle session with a `421`: with nothing to send, nothing connects again (checked for 60 s). Up to 7.25.20 the C_TCP reconnected by itself and logged in with nothing to send (the branch had put it off to `timeout_retry_max`, 3 s here) |
| `pause_backoff` | 7843 | the server is down at the first connection, closes the second, and at the third the emailsender is paused and played during the handshake: the connections come at least 2 s and 4 s apart. Up to 7.25.20 each play connected at once |
| `banner_refused` | 7840 | the server greets with `554 5.7.1 ... client host blocked`: the session's `EV_ON_CLOSE` carries `refused: 554` and the reply, and no credentials are sent. Before, it was retried, paced, for ever |
| `ehlo_refused` | 7841 | the server answers EHLO with `550 5.7.1`: `refused: 550` on `EV_ON_CLOSE`, no credentials sent |
| `auth_334_continue` | 7844 | the server answers `AUTH PLAIN` with `334`, then the response on its own line with `235`: delivered, one login. In the branch before this fix the `334` stopped the yuno |
