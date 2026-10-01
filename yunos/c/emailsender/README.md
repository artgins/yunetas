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
- **Delivery is at least once, not exactly once.** A message is done only
  when the server answers `250` to the final `.` of its DATA. If that answer
  does not reach us -- the connection is lost after the `.` was sent (a RST, a
  TLS error, a close), the `250` does not come within `timeout_response`, the
  reply line is malformed, or the yuno is paused, stopped or killed in that
  instant -- the server may have taken the message all the same, and we
  cannot know. It is a failure of the transaction: the message stays at the
  head of the queue and is sent again (a pause, a stop or a kill spends no
  retry, the others spend one), so **the recipient can get it twice**. This
  is the window of RFC 1047 (*Duplicate messages and SMTP*), and nothing on
  the client side closes it. When it was the last of the message's
  `max_retries`, the message goes to the failed queue though it may have
  been delivered.
- **Permanent failures** (a `5xx` to every RCPT TO, to DATA or to the end of
  DATA: `550 5.1.1` no such user, `554` content refused) → the message goes
  **straight to `emails_failed`**, tried once. It is the message's fault, not
  the server's: `C_SMTP_SESSION` answers it at once on `EV_ON_MESSAGE`
  `{ok: false, code, permanent: true}` and the session goes on -- it sends
  `RSET`, and the next message goes on the same connection, at once. A server
  that refuses `RSET` with a `5xx` is told `QUIT`, and the next connection
  waits `timeout_retry` (a server that closes after each refusal is not logged
  in to once per queued message). That is no failure of the server: no
  *"SMTP server failing"*, the streak (see below) as it was -- **for the first
  two refusals in a row**. From the third refusal with no delivery between
  them the server is refusing every message (a blocked account, a quota, a
  policy): each refusal still sends its message to the failed queue, but the
  session is dropped as for a failure and the next message waits the paced
  delay, and the run is said once, *"SMTP server refused the last messages in
  a row: each goes to the failed queue, the next ones are paced"*, with
  `refused_in_row`. There is no ERROR of `timeout_failing_alarm` for it (the
  emails are not stuck: each one's ERROR *"moved to failed queue"* says it).
  Every refusal counts, a bad address included (`550 5.1.1` no such user, a
  sender of the message's own): a queue refused one by one -- a server that
  takes no foreign sender, a batch to a dead address, a server that refuses
  `RSET` and is told `QUIT` each time -- must not get one login per message.
  The price: a good email behind a run of refused ones waits the paced delay,
  until the next delivery ends the run. So a
  queue of messages the server refuses one by one costs one attempt per
  message, at the paced rate: with `timeout_retry` 1 s, 20 such messages see
  four connections in the first 10 s or so, not 20 in a second. Up to 7.25.20 a
  `5xx` to RCPT TO or DATA dropped the session, and the next message logged in
  again at once.

  A `5xx` to ONE recipient of several refuses that recipient only: the
  message goes to the others (RFC 5321 §3.3), and each refused one is a
  WARNING (*"RCPT TO rejected"*, with the reply). The *"email sent"* line
  then says who got it and who did not: `to` and `cc` hold only the accepted
  addresses, `refused` the refused `to`/`cc` ones, and the bcc go as counts
  (`bcc_count`, `refused_bcc_count`):

  ```json
  {"msg": "email sent", "to": "", "cc": "copy@example.com", "bcc_count": 0,
   "refused": "reader@example.com", "refused_bcc_count": 0, "url": "..."}
  ```

  Up to 7.25.20 one bad address sent the message to nobody, into the failed
  queue.

  A refused **sender** (MAIL FROM): the REPLY decides, not the `from`. Every
  producer sets the `from` of its emails (the yuno's `from` is only the
  default), so most emails carry a sender of their own, and an account block
  answers them all the same way:

  - **The message's** only when its sender is its own (not the default; the
    comparison ignores case) AND the reply says that address is wrong: a
    `501` (its syntax), a `5.1.7` status (bad sender mailbox syntax, RFC 3463),
    or a `553`, `5.1.x` or `5.7.1` reply that quotes the address itself
    (`553 5.1.8 <a@b>: Sender address rejected: Domain not found`,
    `553 5.7.1 <a@b>: Sender address rejected: not owned by user`).
    Refused like above, once, to the failed queue, with a WARNING that names
    the `from`; the session goes on, and a run of them is paced like any run
    of refusals.
  - **The account's** in every other case, whatever the `from`: a quota
    (`550 5.7.1 Daily sending quota exceeded`), a block (`550 5.1.8 Access
    denied, bad outbound sender`), a policy that does not name the address,
    any `4xx`, any refusal of the default sender. A failure of the server:
    paced, said, and charged to the message as a retry, so after
    `max_retries` paced attempts it goes to the failed queue and the queue
    moves on; a stuck head never blocks the queue for ever, and one stuck
    past `timeout_failing_alarm` raises the ERROR. The yuno does not stop on
    it: a quota passes.

  Every attempt that ends in a refusal of the message either sends it to the
  failed queue or spends one of its retries, whatever the session was doing.
  Up to 7.25.20 any `5xx` there sent the message to the failed queue at once,
  and every message behind it in turn.

  To get rid of the email at the head of the queue at once -- the one every
  other waits behind -- without waiting for its paced attempts:

  ```bash
  ycommand -c 'command-yuno id=<id> service=emailsender command=skip-email'
  ```

  It moves that email to the failed queue (a WARNING, *"email skipped by
  command: moved to failed queue"*) and answers with its `to` and `subject`;
  the next email is tried at once if the pace allows it. It works while
  paused too (the queues are opened for it).

  A `4xx` to the message -- a rate limit, a greylist, a `421` -- IS server
  trouble: the session is dropped, the reconnection paced, and the code goes
  up on `EV_ON_CLOSE` for a retry.
- **Bad content** (MIME build / recipient parse failures) → permanent, dead-letter.
  A message the session cannot send at all (no valid recipient, no sender) is
  answered once, with `EV_ON_MESSAGE` `{ok: false, permanent: true}`; a send the
  session does not take (`-1`) is not answered, and the message waits at the
  head of the queue, no retry spent. Up to 7.25.20 the refusal was answered AND
  returned -1, and the message was resolved twice. Any number of such
  messages in a row (a batch persisted by a broken sender) is drained by a
  loop, with the stack where it is: up to 7.25.20 each one was sent from
  inside the resolution of the one before, about 1 KB of stack each, and some
  9000 of them overflowed an 8 MB stack.
- **Rejected credentials** (a `5xx` to `AUTH PLAIN`: `535`, `534`, `530`,
  `538`, ...) → the yuno logs one ERROR, with the server's reply text, and
  **exits with code 0** (`LOG_OPT_EXIT_ZERO`), so neither the watcher nor the
  agent relaunches it: retrying wrong credentials is what gets the node's
  address banned by the mail provider. `C_SMTP_SESSION` reports it on
  `EV_ON_CLOSE` as `auth_rejected` (the reply code), apart from `code`: the
  message was never seen by the server, so it stays queued without spending a
  retry.
- **A `334` to `AUTH PLAIN`**: we send the credentials with the command, as
  RFC 4954 allows, and the RFC also lets the server ask for them on a line of
  their own with a `334`. The session gives that line once, the same
  credentials -- still a single login, not a probe. A second `334` is handled
  like rejected credentials (`auth_rejected: 334`, the exit says *"SMTP server
  does not take AUTH PLAIN"*): it would repeat at every connection.
- **A server that refuses this client** -- a `5xx` to the greeting (`554` of a
  provider that blocked the address) or to EHLO -- stops the yuno the same
  way, with one ERROR (*"SMTP server refuses this client: exiting, NOT
  relaunched"*, with the reply): every attempt would meet the same answer and
  be seen by the provider. `C_SMTP_SESSION` reports it on `EV_ON_CLOSE` as
  `refused` (the code), and no credentials are sent. Before, it was retried,
  paced, for ever (7.25.20: four attempts per message, then the failed
  queue).
- **A transient refusal of the login** (a `4xx` to `AUTH PLAIN`: `454`
  temporary authentication failure, `421`, `432`, ...) says nothing of the
  credentials: the session closes like any drop, and the login is tried again
  at the next connection, paced. The in-flight message spends no retry: the
  server never saw it. (Up to 7.25.20 it was taken as a refusal, and a brief
  outage of the provider stopped the yuno for good while the queue piled up.)
- **Retries are paced -- every one of them.** After a failed session or
  connection the next connection waits: a `4xx` refusal of the login or of the
  message, the default sender refused, a run of refused messages (above), a `4xx` or a
  `421` to the end of DATA (a rate limit, a greylist), a reply that never
  comes, a malformed or over-long reply, a server that closes the connection
  by itself (a RST, a TLS error, a close with no reply), a connection that is
  refused, times out, or whose TCP connect or TLS handshake does not end
  within `timeout_response` (up to 7.25.20 nothing watched them: a server that
  took the connection and stalled TLS kept the message waiting for ever, with
  nothing logged). The wait is `timeout_retry` ms
  (default `2000`), twice as long after each further failure in a row, up to
  `timeout_retry_max` (default `600000`). An email queued during the wait waits
  for it too, and so does a pause and a play of the yuno: a stop in the middle
  of a failing streak counts as one more failure. The doubling starts again
  from `timeout_retry` after a session that ends with no failure -- a message
  delivered, an idle session closed by either side --, so a server that works
  is not held to the delay of an old outage; after such a close a message
  waiting is sent on a new connection as soon as the transport is down, also
  when it comes while the transport is still closing (it is not begun on the
  closing connection, which used to be counted as a failure of the server and
  a retry of the message). A connection that cannot be made (refused, timed
  out, a TLS handshake that fails) is a failure like the others: the `C_TCP`
  publishes nothing for it, and the session sees it as the `C_TCP`'s
  `EV_STATE_CHANGED` out of `ST_WAIT_CONNECTED`, with the cause the `C_TCP`
  writes in its `disconnect_cause`.

  A url changed (`set-url-from`, then a pause and a play) starts the pacing
  and the streak afresh: they were the old server's. Up to 7.25.20 the
  corrected server waited the old one's delay, up to `timeout_retry_max`.

  `timeout_retry` below 1000 ms, `timeout_retry_max` below `timeout_retry`
  and `timeout_response` below 1000 ms are refused with an ERROR, and the
  default is taken: a `timeout_retry` of 0 reconnected in a tight loop after
  each failed handshake, and a timer of 0 arms nothing.
- **Nothing connects with nothing to send.** The `C_TCP` never connects by
  itself: not when it starts (`connect_on_start` false), not after a close
  (`timeout_between_connections` -1). Every connection is the session's, made
  when it holds a message: a yuno that starts or plays with an empty queue
  logs in to nobody, and after any close -- an idle session the server ends
  (OVH closes them early, with a `421`), a failure with nothing queued behind
  it -- nothing logs in again until an email comes. Up to 7.25.20 the `C_TCP`
  connected at every start of the session and reconnected 2 s after any close
  it did not decide, and logged in with nothing to send, for ever.
- **A failing server is said, once, then loud.** The first failure of a streak,
  while an email waits, is a WARNING, *"SMTP server failing: emails wait, the
  retries are paced"* (or, for a run of refused messages, *"SMTP server
  refused the last messages in a row"*, which never becomes the ERROR), with its
  `cause` (the reply of the server, or *"cannot connect: Connection
  refused"*, which the `C_TCP` itself logs only when traced). When the streak
  has lasted `timeout_failing_alarm` (default `3600000`, 1 h; `0`: never) it is
  an ERROR, *"SMTP server failing for too long: emails are NOT being sent"*,
  said again at most once per that period while it lasts -- at the retries,
  with no timer of its own. The first delivery ends a streak with an INFO,
  *"SMTP server works again: emails delivered"* (`failed_s`); a streak of
  connections and handshakes only ends at the next handshake that works,
  *"SMTP server answers again"*; any streak ends at a session closed with no
  failure. With no email waiting there is no streak and nothing is said. Up
  to 7.25.20 a server that could not be reached left no trace at the default
  levels.

  A message spends one of its `max_retries` per failure of ITS transaction
  (its first RCPT TO onwards: a `4xx`, a close or a timeout there; and the
  default sender refused at MAIL FROM; the session tells it with
  `transaction` on `EV_ON_CLOSE`). A failure before it -- the connection,
  the greeting, EHLO, a transient AUTH -- spends none: the
  server never saw the message, which waits at the head of the queue for as
  long as the outage lasts, paced. Up to 7.25.20 each of those spent a retry
  too (all but a connection that could not be made at all), 2 s apart: a
  server that failed its handshake sent the message at the head of the queue
  to the failed queue in about 8 s, then the next one, for as long as it
  failed.

  **The exact schedule.** The wait after the k-th failure in a row is
  `timeout_retry` × 2^(k-1), capped at `timeout_retry_max`: with the
  defaults 2, 4, 8, 16, 32, 64, 128, 256 and 512 s, then 600 s after every
  further failure. "In a row" counts every failure of the session, of any
  message and of any kind -- a failure that spends no retry (a refused
  connection, a `421` greeting) takes the next step too. A message refused
  with a `5xx` takes no step and resets nothing -- the first two in a row;
  from the third on each refusal is a failure and takes a step.
  The count goes back to the start only after a delivery, or after a session
  that ends with no failure (an idle session closed by either side). Moving a
  message to the failed queue does NOT reset it: the next message goes on
  from where the count stands. So with the defaults (`max_retries` 4), against
  a server that refuses every message with a `4xx`:

  | Message | Wait before its first attempt | Waits between its four attempts |
  |---------|-------------------------------|---------------------------------|
  | the first after a delivery or a start | none | 2 + 4 + 8 = 14 s |
  | the second | 16 s | 32 + 64 + 128 = 224 s |
  | the third | 256 s | 512 + 600 + 600 = 1712 s (28.5 min) |
  | the fourth and later | 600 s | 3 × 600 = 1800 s (30 min) |

  (The connection and the session of each attempt add their own time to
  these waits.) A batch config for a provider known to refuse for long raises
  `max_retries`: with `'max_retries': 10` the first message's waits add up to
  2 + 4 + ... + 512 = 1022 s, about 17 minutes, and a message later in the
  same streak gets 9 × 600 s = 90 minutes:

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
  (the log reaches the logcenter).
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
begins the stashed message on entry to `ST_IDLE`. The `C_TCP` never reconnects
by itself. The pacing is the session's concern, not the sender's.

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
(also `remove-emails-failed` to drain the dead-letter queue, and `skip-email`
to move the email at the head of the queue there).

## Build & deploy

Standard yuneta build (`make install` from the yuno's `build/`
directory) followed by the agent's `install-binary` + `create-yuno` +
`run-yuno` cycle. See
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

Production deployments live in the private `estadodelaire` repo
under its `batches/<host>/` tree.
