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
  (anything else logs "Fake smtp: message refused"; `hold` answers nothing,
  "Fake smtp: DATA held", and leaves the client in its transaction), and after
  a `421` the server closes the connection. `connection_plan` says what it does with each
  connection: `greet`, `drop` (closed at once), `garbage` (two malformed lines
  in one write) or `long_line` (a line longer than the client's reply buffer).
  `MAIL FROM` is answered from `mail_replies`, `RSET` from `rset_replies`, and with `max_connections` one
  connection more is an ERROR ("Fake smtp: one connection too many").
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
end. A failure the SMTP server causes is a WARNING, not an ERROR. The session
connects only when it holds an email, so no test sees a connection at the
start of the emailsender; a test whose streak of failures is of connections
and handshakes only sees it end at the next handshake that works ("SMTP
server answers again"), and one with a failure of a transaction in it at the
delivery ("SMTP server works again").

A test whose yuno exits by itself (`LOG_OPT_EXIT_ZERO`, as `C_EMAILSENDER`
does on a refused login) never reaches its checks: an `atexit()` guard turns
that exit into a failure.

## Tests

| Test | Port | What it proves |
|------|------|----------------|
| `auth_transient` | 7821 | the server answers the first `AUTH` with `454 4.7.0 Temporary authentication failure` and the second with `235`: the message is delivered, no retry spent (the server never saw it). Up to 7.25.20 any answer but `235` was taken as rejected credentials, and the yuno exited with 0, not to be relaunched |
| `auth_rejected` | 7822 | a `C_SMTP_SESSION` of the driver's own gets `535 5.7.8 ...`: its `EV_ON_CLOSE` carries `auth_rejected: 535` and the reply text in `reply`, which up to 7.25.20 it did not carry |
| `set_email_user` | 7823 | the service starts with no credentials and a url where nobody listens (7844); `set-email-user` gives it the credentials and the url of the fake server, and the email is delivered THERE. Up to 7.25.20 the SMTP side was started before the url was written, and on the url its session was created with |
| `set_url_running` | 7824 | the service starts WITH credentials and the dead url, so its session runs; `set-url-from` gives it the url of the fake server (its answer says the url waits for the next start), the service is paused and played, and the email is delivered at the new url. Up to 7.25.20 the session kept the url it was created with until the yuno was restarted. Here the fake server is not the service's `__input_side__` (a pause stops that one): the driver starts it |
| `holds_through_handshake` | 7825 | (was `send_in_handshake`) the email waits in the SMTP session through its handshake (the server greets 1.5 s after the connection) and is begun when the session is idle. The session connects only for a message, so an email can no longer reach it in the middle of a handshake started for nothing; up to 7.25.20 it could, `C_SMTP_SESSION` took `EV_SEND_MESSAGE` only disconnected or idle ("Event NOT DEFINED"), and the email spent its retries in the same instant. `C_SMTP_SESSION` keeps `EV_SEND_MESSAGE` in its handshake states: only an owner that sends a second message while one is in hand reaches them, and it gets the ERROR "EV_SEND_MESSAGE while another message in flight" rather than "Event NOT DEFINED" |
| `auth_backoff` | 7826 | the server answers `454` to three AUTHs, then `235`; with `timeout_retry` 1 s the gaps between the AUTHs must be at least 1, 2 and 4 s, and the email, with `max_retries` 2, is delivered: a failed login spends no retry. Up to 7.25.20 every retry came after the transport's fixed 2 s ("Fake smtp: AUTH too early" at the fourth), and each failed login spent a retry |
| `pause_in_flight` | 7827 | the emailsender is paused while its email is in flight (the session in its handshake) and played in the next cycle of the loop, its C_TCP still closing: the email is delivered once, no retry spent. Up to 7.25.20 the pause freed the queued message while `qmsg_cur_email` kept pointing at it (the late `EV_ON_CLOSE` resolved freed memory: "email NOT sent, will retry"), and the play restarted a C_TCP still closing ("Initial wrong tcp state") |
| `shutdown_in_flight` | 7830 | the yuno is told to die with the email in flight: it stays queued and nothing is said about it. Up to 7.25.20 the `EV_ON_CLOSE` of the closing session resolved the freed message (a segfault here) |
| `set_url_stash` | 7831 | the email of the play waits in the SMTP session (dead url); `set-url-from` gives it the fake server, and a pause and a play send it once, at the new url. Up to 7.25.20 the waiting message survived the stop of the session and the owner kept a dangling pointer to its queued copy |
| `no_recipients` | 7832 | an `EV_SEND_EMAIL` whose `to` holds no address (`","`, no cc), then a good one: the first goes to the failed queue once (one ERROR), the second is delivered. Up to 7.25.20 the session answered the refusal with `EV_ON_MESSAGE` AND returned -1, and the emailsender resolved the message twice: its retries went in one instant, re-sent from inside the first answer, and the unwinding frames touched it after the failed queue had freed it (a segfault here) |
| `data_4xx_paced` | 7833 | the server answers the end of DATA with `451`, then `421` (and closes), then `250`; the uploads must come at least 1 s and 2 s apart (`timeout_retry` 1 s). Up to 7.25.20 a `4xx` there left the session up and the message was uploaded again at once ("Fake smtp: DATA too early"). It runs with no inactivity timeout, so the C_TCP arms its reconnection after the session's timer: the `EV_CONNECT` of the session must cancel it, or it fires in `ST_WAIT_CONNECTED` ("Event NOT DEFINED in state", twice) |
| `server_close_paced` | 7834 | the server closes the first connection at once, answers the second with two malformed lines in one write, closes the third at once and greets the fourth: the connections come at least 1, 2 and 4 s apart, the third within 3.5 s of the second (two aborts of one read double the delay once), and the email, with `max_retries` 2, is delivered: none of those failures is a retry of it. Up to 7.25.20 a close by the server reconnected after a fixed 2 s |
| `late_server` | 7835 | the server is down for the first 8.5 s; with `timeout_retry` 1 s the connection attempts come at 0, 1, 3, 7 and 15 s, so the session reaches it at least 4 s after it is up. Up to 7.25.20 a refused connection was retried every 2 s for ever (`timeout_retry_max` never reached the C_TCP). And it is said: the WARNING "SMTP server failing" at the first refusal with its cause ("cannot connect: Connection refused", the C_TCP's `disconnect_cause`), the ERROR of `timeout_failing_alarm` (5 s here) once, and the INFO "SMTP server answers again" at the first handshake; up to 7.25.20 nothing was logged at the default levels |
| `refill_paced` | 7836 | the server answers the recipient of the first email with `450` (try again later); with `max_retries` 1 it goes to the failed queue, and the session waits 3 s to reconnect; a second email 1.5 s later must wait for them. Up to 7.25.20 it connected at once ("Fake smtp: connection too early"). It used a `550`, which no longer drops the session (see `rcpt_refused_same_session`) |
| `auth_334` | 7839 | a `C_SMTP_SESSION` of the driver's own gets `334 ` to its `AUTH PLAIN`, answers it once with the same response, and gets `334 ` again: its `EV_ON_CLOSE` carries `auth_rejected: 334` (the emailsender stops on it, as on refused credentials). Up to 7.25.20 the first `334` already stopped the yuno: any reply but `235` was taken as rejected credentials |
| `long_line` | 7837 | the server greets the first connection with a line longer than the reply buffer of the session: one WARNING ("SMTP reply line too long"), no ERROR, and the email is delivered on the next connection. Up to 7.25.20 filling the istream logged "gbuf FULL" (and two gbuffer errors) as ERRORs with a stack |
| `url_log` | 7838 | the running emailsender is given a dead url with `set-url-from`, then sends one email: the session goes on with the url it runs on, and "email sent" must name that url. Up to 7.25.20 it named the new url, where nothing had been sent |
| `idle_close_no_reconnect` | 7846 | after the delivery the server ends the idle session with a `421`: with nothing to send, nothing connects again (checked until the yuno ends, 6 s after the delivery: twice `timeout_retry_max`). Up to 7.25.20 the C_TCP reconnected by itself 2 s later and logged in with nothing to send; the 6 s window also catches a reconnection put off to `timeout_retry_max` (3 s here) |
| `pause_backoff` | 7847 | the server closes the first connection, and at the second the emailsender is paused and played during the handshake (a stop in a failing streak: one more failure): with `timeout_retry` 2 s the connections come at least 2 s and 4 s apart. Up to 7.25.20 each play connected at once |
| `banner_refused` | 7840 | the server greets with `554 5.7.1 ... client host blocked`: the session's `EV_ON_CLOSE` carries `refused: 554` and the reply, and no credentials are sent. Before, it was retried, paced, for ever |
| `ehlo_refused` | 7845 | the server answers EHLO with `550 5.7.1`: `refused: 550` on `EV_ON_CLOSE`, no credentials sent |
| `auth_334_continue` | 7848 | the server answers `AUTH PLAIN` with `334`, then the response on its own line with `235`: delivered, one login. Up to 7.25.20 the `334` stopped the yuno, as rejected credentials |
| `bad_burst` | 7849 | one good email, and queued behind it while it is in flight 2000 with no recipient: when the good one is delivered all 2000 reach the failed queue (`list-queues`), each one's ERROR logged at the same stack depth (the span of the stack between them must stay under 64 KB; the log handler measures it). Up to 7.25.20 each one was sent from inside the resolution of the one before: 1.9 MB of stack for 2000, an overflow of an 8 MB stack at some 9000 |
| `rcpt_refused_same_session` | 7850 | two emails; the server refuses both recipients of the first with `550 5.1.1`: it goes to the failed queue, the session sends `RSET`, and the second is delivered on the same connection at once (`max_connections` 1, `timeout_retry` 5 s), with no "SMTP server failing" WARNING. Before, the session was dropped and the second email waited `timeout_retry` for a second connection |
| `rset_refused` | 7851 | the same, but the server refuses `RSET` too (`502`): the session says `QUIT`, and the second email connects at least 1 s and at most 3 s after the first connection (`timeout_retry` 1 s): a server that closes after each refusal is not logged in to once per message. Before, it connected in the next cycle of the loop |
| `refusing_every_message` | 7852 | twenty emails, every recipient refused (`550 5.7.1`): the first two go on at once on one session (`RSET`); from the third each refusal drops the session as a failure, with one WARNING "SMTP server refused the last messages in a row", and the connections come 1, 2, 4 s apart (`timeout_retry` 1 s, `max_connections` 4). After 13 s (the timers tick by the second), six are in the failed queue and fourteen wait. Before, all twenty went to the failed queue in a second, on one session, with nothing said |
| `mail_from_refused` | 7853 | the server refuses the default sender once for the account (`451 4.7.1` sending rate): a failure of the server ("SMTP server failing" with the reply), paced (1 s), charged as a retry (`max_retries` 2), and the email is delivered on the second connection. Before the sender rules, a 5xx there sent every queued email to the failed queue |
| `partial_rcpt` | 7859 | the server refuses `to` (`550`) and takes `cc`: the message is delivered to `cc`, the refusal a WARNING, and "email sent" says who got it: `to` "" and `cc` the accepted ones, `refused` "reader@example.com" (before this, it listed every recipient as if all had got it). Before, it went to nobody, into the failed queue |
| `timeout_retry_min` | 7856 | the emailsender is given `timeout_retry` 0: an ERROR, and the default (2 s) taken; the server drops two connections, and the next ones come at least 2 s and 4 s apart. Before, 0 was taken and each failed handshake reconnected at once ("connection too early") |
| `exit_on_refused` | 7857 | the server greets with `554`: the emailsender logs its ERROR and exits with code 0 (`LOG_OPT_EXIT_ZERO`, not relaunched); an `atexit()` handler checks the ERROR and that `list-queues` still shows the email pending, none failed |
| `connect_watchdog` | 7861 | the emailsender connects (`smtps://`) to a socket that listens and never accepts (7858): TCP connects, TLS never answers. With `timeout_response` 2 s the attempt is dropped and said ("SMTP server failing", cause "cannot connect: no connection, or no TLS handshake, within timeout_response"), and the email stays queued. Before, nothing watched the connect: the email waited for ever, with nothing logged |
| `url_change_resets_pacing` | 7855 | the email waits on a dead url (7860), failing at 0, 1, 3 and 7 s; at 7.5 s `set-url-from` gives the fake server and a pause and a play: the session connects within 1.8 s of the play (the timers tick by the second). Before, the new server waited the old one's backoff (about 7.5 s more here) |
| `foreign_from_refused` | 7862 | two emails, the first with a `from` of its own refused `553 5.1.8 <intruder@example.com>: ... Domain not found` (the reply quotes that address): the message's, to the failed queue once (the WARNING names the `from`), and the second (default `from`) is delivered on the same session (`max_connections` 1) |
| `default_from_refused` | 7863 | the default sender is refused (`550 5.7.1` quota): paced (gaps >= 1 s, 2 s) and charged to the message, which with `max_retries` 3 goes to the failed queue after its third attempt. Before, it was retried for ever with no retry spent |
| `skip_email` | 7864 | the default sender is refused and the email waits its paced retry; `skip-email` moves it to the failed queue at once (WARNING), answers with its `to` and subject, and `list-queues` shows none pending, one failed |
| `quota_foreign_from` | 7865 | two emails with a `from` of their own and an account at its quota (`550 5.7.1 Daily sending quota exceeded`, no address quoted): the account's, whatever the from -- paced, a retry spent per attempt; with `max_retries` 2 the first is in the failed queue after two attempts and the second waits, and the stuck head raises the ERROR of `timeout_failing_alarm` (2.5 s). Before, a sender of its own made it the message's: both failed in milliseconds |
| `foreign_refused_batch` | 7866 | twelve emails with a `from` of their own whose domain the server does not know (`553 5.1.8 <intruder@example.com>: ... Domain not found`): each is the message's, but the run is paced -- after 6.5 s five failed, seven wait, three connections (`max_connections` 3). Before, those refusals did not count: twelve in milliseconds |
| `refusal_then_sender` | 7867 | three emails refused at RCPT TO (`550 5.7.1`), then the default sender of the fourth refused at MAIL FROM: the fourth still spends a retry per attempt and is in the failed queue after `max_retries` 2, and the stuck head raises the ERROR. Before, a refusal in a run dropped the session with no transaction: the fourth was sent again for ever, with the ERROR silenced |
| `rset_refused_logins` | 7868 | twelve emails to a dead address (`550 5.1.1`) and a server that refuses `RSET`: each refusal ends the session with `QUIT`, and the run is paced: at most five connections in 8 s (`max_connections` 5). Before, a bad address did not count: a login a second, one per email |
| `quoted_quota` | 7869 | twelve emails with a `from` of their own and a quota reply that QUOTES the address (`550 5.7.1 <alarm@example.com>: sending quota exceeded`): a 5.7.x is the account's, quoted or not -- paced, a retry per attempt, one email in the failed queue after `max_retries` 3, eleven waiting after 9 s, and the ERROR of the alarm. Before, the quoted own from made it the message's: one email failed per paced reconnection |
| `quoted_access` | 7870 | the same with Postfix's sender reject (`554 5.7.1 <alarm@example.com>: Sender address rejected: Access denied`): the account's, the same counts |
| `set_user_queued` | 7871 | the service starts with no credentials and a url where nobody listens (7872); an email is queued, THEN `set-email-user` gives the credentials and the url of the fake server, and no other email follows: the queued one is delivered. Up to 7.25.21 `set-email-user` only started the session, which connects only for a message it holds, and the email waited for the next one queued |
| `skip_in_flight` | 7873 | two emails; the server holds the end of DATA of the first (`data_replies` `hold`), and `skip-email` moves it to the failed queue while the session is in its mail transaction: the second is delivered. Up to 7.25.21 the stopped session, still in its transaction state until its C_TCP closed, refused the second ("Event NOT DEFINED"), and the queue waited for the next email queued. The fake server has two channels: the session reconnects before the server sees the first close |
| `skip_no_credentials` | 7874 | the service plays with no credentials (one ERROR says so) and an email waits; `skip-email` moves it to the failed queue and nothing else is logged. Up to 7.25.21 `skip-email` started the SMTP side whatever it was before, and repeated the "username or password is empty" ERROR at every skip |
