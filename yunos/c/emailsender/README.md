# emailsender

Yuno that sends emails on behalf of other Yuneta services. Speaks
native SMTPS (implicit TLS, RFC 8314) on top of `C_SMTP_SESSION` +
`C_TCP`, with its own MIME encoder. Queues outgoing messages with
TimeRanger2 persistence and processes them asynchronously.

> Since 7.4.3 the yuno no longer links libcurl; it builds fully
> static like every other yuno in the suite.

## SMTP credentials

A batch config leaves `username` and `password` **blank**. They are set once,
on the running yuno, with `set-email-user`, which saves them as persistent
attrs; persistent attrs are loaded after the config at every start, so they
win over the blank batch values from then on.

```bash
ycommand -c 'command-yuno id=<id> service=emailsender command=set-email-user username=<user> password=<password>'
```

With either one blank the SMTP side does **not** start: the yuno runs, accepts
and queues emails, and logs one ERROR (*"SMTP username or password is empty:
emails are queued, NOT sent. Set them with the set-email-user command"*).
`set-email-user` applies at once, with no restart: it hands the credentials to
the SMTP session, starts it, and the queue is sent. A `url=` (and a `from=`)
given with them is written first, so the session starts on that url:

```bash
ycommand -c 'command-yuno id=<id> service=emailsender command=set-email-user username=<user> password=<password> url=smtps://ssl0.ovh.net:465'
```

A url changed while the SMTP session is already running (`set-email-user` or
`set-url-from` on a yuno that was sending) is taken when the session starts
again -- pause and play the yuno -- and the command's answer says so: the
session hands the new url to its `C_TCP` in its `mt_start`, with a fresh copy
of its `crypto` (the one `C_TCP` used holds the TLS server name of the old
host). Up to 7.25.20 the session kept the url it was created with until the
yuno was restarted, and `set-email-user ... url=` even started it on the old
one. (Before this, a blank
password made the service refuse to start, since both attrs were
`SDF_REQUIRED`, so `set-email-user` had nothing to talk to.)

## SMTP send path — retries, dead-letter, reconnection

Outgoing mail lives in the persistent `emails_queue` topic and is sent one at a
time over a single `C_SMTP_SESSION`. The error handling (hardened 2026-05-29):

- **Transient failures** (4xx reply, per-command timeout, mid-transaction
  disconnect) → the message stays at the head of the queue and is retried, up to
  `max_retries` (default 4); then it is moved to the `emails_failed` dead-letter
  queue.
- **Permanent failures** (any `5xx` to MAIL FROM / RCPT TO / DATA / the body) →
  the message goes **straight to `emails_failed`**, no retries. The SMTP reply
  code is carried up from `C_SMTP_SESSION` on `EV_ON_CLOSE`: every refusal,
  the one of the end of DATA included, drops the session; `code in [500,600)`
  ⇒ permanent. `EV_ON_MESSAGE` answers a delivery (and a message that cannot
  be sent at all).
- **Bad content** (MIME build / recipient parse failures) → permanent, dead-letter.
  A message the session cannot send at all (no valid recipient, no sender) is
  answered once, with `EV_ON_MESSAGE` `{ok: false, permanent: true}`; a send the
  session does not take (`-1`) is not answered, and the message waits at the
  head of the queue, no retry spent. Up to 7.25.20 the refusal was answered AND
  returned -1, and the message was resolved twice.
- **Rejected credentials** (a `5xx` to `AUTH PLAIN`: `535`, `534`, `530`,
  `538`, ...) → the yuno logs one ERROR, with the server's reply text, and
  **exits with code 0** (`LOG_OPT_EXIT_ZERO`), so neither the watcher nor the
  agent relaunches it: retrying wrong credentials is what gets the node's
  address banned by the mail provider. `C_SMTP_SESSION` reports it on
  `EV_ON_CLOSE` as `auth_rejected` (the reply code), apart from `code`: the
  message was never seen by the server, so it stays queued without spending a
  retry.
- **A `334` to `AUTH PLAIN`** is handled the same way (`auth_rejected: 334`,
  and the exit says *"SMTP server does not take AUTH PLAIN with its initial
  response"*). We send the credentials with the command, as RFC 4954 allows;
  a server that answers `334` wants another exchange, which this client does
  not speak. It is how the server is configured, it repeats at every
  connection, and each one is another failed login in its logs: retrying it,
  even paced, is the wrong answer, so the yuno stops, loud, until the url or
  the server is changed.
- **A transient refusal of the login** (a `4xx` to `AUTH PLAIN`: `454`
  temporary authentication failure, `421`, `432`, ...) says nothing of the
  credentials: the session closes like any drop, and the login is tried again
  at the next connection, paced. The in-flight message spends no retry: the
  server never saw it. (Up to 7.25.20 it was taken as a refusal, and a brief
  outage of the provider stopped the yuno for good while the queue piled up.)
- **Retries are paced -- every one of them.** After a failed session or
  connection the next connection waits: a refusal of the login or of the
  message, a `4xx` or a `421` to the end of DATA (a rate limit, a greylist), a
  reply that never comes, a malformed or over-long reply, a server that closes
  the connection by itself (a RST, a TLS error, a close with no reply), a
  connection that is refused or times out. The wait is `timeout_retry` ms
  (default `2000`), twice as long after each further failure in a row, up to
  `timeout_retry_max` (default `600000`). An email queued during the wait waits
  for it too. The doubling starts again from `timeout_retry` after a session
  that ends with no failure -- a message delivered, an idle session closed by
  either side --, so a server that works is not held to the delay of an old
  outage. A connection that cannot be made never reaches `C_SMTP_SESSION`: its
  `C_TCP` doubles the delay itself, up to the same `timeout_retry_max`.

  A message spends one of its `max_retries` per failure of ITS transaction
  (MAIL FROM onwards: a refusal, a `4xx`, a close or a timeout there; the
  session tells it with `transaction` on `EV_ON_CLOSE`). A failure before it
  -- the connection, the greeting, EHLO, a transient AUTH -- spends none: the
  server never saw the message, which waits at the head of the queue for as
  long as the outage lasts, paced. Up to 7.25.20 each of those spent a retry
  too, and an outage of 14 s sent every queued message to the failed queue in
  turn. So `max_retries` and the pacing together say how long a server that
  keeps refusing a message is given: with the defaults, four attempts are
  spread over 2 + 4 + 8 = 14 s; a batch config for a provider known to
  refuse for long raises `max_retries` (`'max_retries': 10` covers about 17
  minutes):

  ```json
  "kw": {
      "max_retries": 10,
      "timeout_retry": 2000,
      "timeout_retry_max": 600000
  }
  ```

  Up to 7.25.20 every retry came after the transport's fixed 2 s, even failing
  again and again (the four attempts of a message were gone in about 8 s), and
  a `4xx` to the end of DATA sent the message again at once, on the same
  session: four uploads in the same second.
- **An idle session the server ends** (OVH closes idle sessions early, with a
  `421`) is no failure, and there is nothing to send: the transport's own
  reconnection is put off to `timeout_retry_max`, and the next email connects
  at once. Up to 7.25.20 it logged in again 2 s later, and again at each idle
  close of the server, for nothing.
- **Why a session closed**: every close caused by a reply of the server
  (a refused login, a refused message, a failed EHLO, ...) carries that reply's
  text on `EV_ON_CLOSE` as `reply`, next to its code.
- **Severity**: a session the server ends -- a refusal, a `4xx`, an unexpected
  or malformed reply, a reply that never comes -- is a **WARNING** of the
  `Protocol` msgset from `C_SMTP_SESSION`, with the reply (capped at 512 bytes).
  An ERROR from `C_SMTP_SESSION` is our own failure (no memory, an encoder).
  The ERRORs of the emailsender are not about who failed but about what an
  operator must act on, whoever caused it: `email NOT sent, moved to failed
  queue` (an email that will not be delivered unless somebody sends it again)
  and the exit on a refused login (the yuno stops). A retry is a WARNING.
- **What the logs name**: `to` and `cc`, the `url` of the server the message
  was sent to (the one the session runs on, which a `set-url-from` does not
  change until the next start: up to 7.25.20 the log named the new url), and
  `bcc_count` -- never the `bcc` addresses, which are hidden by definition
  (in the branch they were written to the log, and from there to the
  logcenter).
- **Binary bodies**: a non-UTF-8 body is persisted base64 under `body_base64`
  (a plain `json_string` would silently drop it) and decoded at send time.

**Layering — who reconnects:** `c_emailsender` does NOT manage reconnection (no
timers up here). It just enqueues and dispatches the head message
(`EV_SEND_MESSAGE`) whenever it is playing, regardless of link state. The bottom
`C_TCP` runs with `timeout_inactivity` (closes the idle SMTP link), so when the
link is down it is **`c_smtp_session`** — the gclass that owns the transport and
must redo the SMTP handshake — that reconnects on demand: on `EV_SEND_MESSAGE`
in `ST_DISCONNECTED` it kicks its bottom `C_TCP` (`EV_CONNECT`) once the paced
delay has passed (before, on its own timer), re-runs banner→EHLO→AUTH, and
begins the stashed message on entry to `ST_IDLE`. The pacing is the session's
and its transport's concern, not the sender's.

On that entry to `ST_IDLE` the child also publishes `EV_ON_OPEN` *before* it
begins the stashed message. Because `c_emailsender` moves to `ST_WAIT_RESPONSE`
**before** dispatching (`EV_SEND_MESSAGE`), that `EV_ON_OPEN` lands while the
parent is already in `ST_WAIT_RESPONSE` — so `ST_WAIT_RESPONSE` accepts
`EV_ON_OPEN` as a no-op (just marks the link ready; it does **not** re-dequeue,
a message is already in flight). The send completes normally and the following
`EV_ON_MESSAGE` resolves it. Omitting that handler made every
reconnect-to-deliver cycle log a spurious *"Event NOT DEFINED in state"* even
though the mail was delivered.

**Pause and play.** A pause (or the stop of the yuno) with a message in
flight leaves it at the head of `emails_queue`, with no retry spent: the
session drops what it holds and says nothing of the connection it closes, and
the next play sends the queue again -- also when the play comes right after the
pause, while the old connection is still closing (the session starts its
`C_TCP` when that close ends). Up to 7.25.20 the pause freed the message while
the emailsender kept pointing at it, and a queued message waited for the next
email to be sent after a play.

Inspect the queues at runtime: `ycommand command-yuno id=<id> command=list-queues`
(also `remove-emails-failed` to drain the dead-letter queue).

## Build & deploy

Standard yuneta build (`make install` from the yuno's `build/`
directory) followed by the agent's `install-binary` + `create-yuno` +
`run-yuno` cycle. See
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

Production deployments live in the private `estadodelaire` repo
under its `batches/<host>/` tree.
