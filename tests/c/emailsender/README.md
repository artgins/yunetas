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
  delivery.
- **Driver**: `C_TEST_EMAILSENDER`, one `scenario` per test (see its header).

A test whose yuno exits by itself (`LOG_OPT_EXIT_ZERO`, as `C_EMAILSENDER`
does on a refused login) never reaches its checks: an `atexit()` guard turns
that exit into a failure.

## Tests

| Test | Port | What it proves |
|------|------|----------------|
| `auth_transient` | 7821 | the server answers the first `AUTH` with `454 4.7.0 Temporary authentication failure` and the second with `235`: the message spends one retry and is delivered. Up to 7.25.20 any answer but `235` was taken as rejected credentials, and the yuno exited with 0, not to be relaunched |
| `auth_rejected` | 7822 | a `C_SMTP_SESSION` of the driver's own gets `535 5.7.8 ...`: its `EV_ON_CLOSE` carries `auth_rejected: 535` and the reply text in `reply`, which up to 7.25.20 it did not carry |
| `set_email_user` | 7823 | the service starts with no credentials and a url where nobody listens (7829); `set-email-user` gives it the credentials and the url of the fake server, and the email is delivered THERE. Up to 7.25.20 the SMTP side was started before the url was written, and on the url its session was created with |
| `set_url_running` | 7824 | the service starts WITH credentials and the dead url, so its session runs; `set-url-from` gives it the url of the fake server (its answer says the url waits for the next start), the service is paused and played, and the email is delivered at the new url. Up to 7.25.20 the session kept the url it was created with until the yuno was restarted. Here the fake server is not the service's `__input_side__` (a pause stops that one): the driver starts it |
