# emailsender

emailsender is a yuno that sends emails for other Yuneta services. It keeps
the outgoing emails in a persistent queue and sends them over SMTPS.

## Usage

The batch configuration of the yuno leaves the SMTP `username` and `password`
blank. Set them on the running yuno:

```bash
ycommand -c 'command-yuno id=<id> service=emailsender command=set-email-user username=<user> password=<password>'
```

To set the SMTP server in the same command, add `url=` (and, if necessary,
`from=`):

```bash
ycommand -c 'command-yuno id=<id> service=emailsender command=set-email-user username=<user> password=<password> url=smtps://ssl0.ovh.net:465'
```

To see the queues at runtime, run `ycommand command-yuno id=<id> command=list-queues`.

To move the email at the head of the queue to the failed queue, run
`skip-email`:

```bash
ycommand -c 'command-yuno id=<id> service=emailsender command=skip-email'
```

To drain the failed queue, run the `remove-emails-failed` command.

The sections below explain what each command does and how the yuno handles
failures.

## What it is

emailsender speaks native SMTPS (implicit TLS, RFC 8314) on top of
`C_SMTP_SESSION` + `C_TCP`, with its own MIME encoder. It queues the outgoing
messages with TimeRanger2 persistence and processes them asynchronously.

> Since 7.4.3 the yuno no longer links libcurl. It builds fully static like
> every other yuno in the suite.

## SMTP credentials

A batch configuration leaves `username` and `password` blank. You set them
once, on the running yuno, with `set-email-user` (see Usage). This command
saves them as persistent attrs (attributes that the yuno saves and loads at
each start). The yuno loads the persistent attrs after the configuration at
every start. From then on, they win over the blank values of the batch.

If one of the two is blank, the SMTP side does not start. The yuno runs,
accepts and queues emails, and logs one ERROR (*"SMTP username or password is
empty: emails are queued, NOT sent. Set them with the set-email-user
command"*).

`set-email-user` applies at once, with no restart. It gives the credentials
to the SMTP session and starts the session, and the yuno sends the queue. If
you give a `url=` (and a `from=`) with the credentials, the command writes it
first, so the session starts on that url.

The session can already run when you change the url (`set-email-user` or
`set-url-from` on a yuno that was sending). Then the session takes the new
url when it starts again, and the answer of the command says so. To start it
again, pause and play the yuno. In its `mt_start`, the session gives the new
url to its `C_TCP`, with a new copy of its `crypto`. The copy that `C_TCP`
used holds the TLS server name of the old host.

Up to 7.25.20 the session kept the url that it was created with until the
yuno restarted. `set-email-user ... url=` even started it on the old url.
(Before this, a blank password made the service refuse to start, because
both attrs were `SDF_REQUIRED`. So `set-email-user` had nothing to talk to.)

## SMTP send path: retries, dead-letter, reconnection

Outgoing mail lives in the persistent `emails_queue` topic. The yuno sends it
one message at a time over a single `C_SMTP_SESSION`. Failed messages go to
`emails_failed`, the dead-letter queue (the queue of the messages that the
yuno does not send again). The error handling (hardened 2026-05-29) is:

- Transient failures (4xx reply, per-command timeout, disconnect in the middle
  of the transaction) → the message stays at the head of the queue. The yuno
  retries it, up to `max_retries` (default 4). Then the yuno moves it to the
  `emails_failed` dead-letter queue.
- Delivery is at least once, not exactly once. A message is done only
  when the server answers `250` to the final `.` of its DATA. That answer can
  fail to reach us. The connection can be lost after the `.` was sent (a RST, a
  TLS error, a close). The `250` can fail to come within `timeout_response`.
  The reply line can be malformed. Or the yuno is paused, stopped or killed in
  that instant. In each of these cases the server can have taken the message
  all the same, and we cannot know. It is a failure of the transaction. The
  message stays at the head of the queue and the yuno sends it again. A pause,
  a stop or a kill spends no retry, the others spend one. So the recipient can
  get the message twice. This is the window of RFC 1047 (*Duplicate messages
  and SMTP*), and nothing on the client side closes it. When it was the last
  of the `max_retries` of the message, the message goes to the failed queue,
  although the server can have delivered it.
- Permanent failures → the message goes straight to `emails_failed`, tried
  once. A permanent failure is a `5xx` to every RCPT TO, to DATA or to the
  end of DATA: `550 5.1.1` no such user, `554` content refused. The fault is in the message, not
  in the server. `C_SMTP_SESSION` answers it at once on `EV_ON_MESSAGE`
  `{ok: false, code, permanent: true}` and the session continues. It sends
  `RSET`, and the next message goes on the same connection, at once. If a
  server refuses `RSET` with a `5xx`, the session sends it `QUIT`, and the
  next connection waits `timeout_retry`. In this way, a server that closes
  after each refusal does not get one login for each queued message. For the
  first two refusals in a row, this is not a failure of the server. The yuno
  logs no *"SMTP server failing"*, and the streak (the series of failures in
  a row, see below) stays as it was.
  From the third refusal with no delivery between them, the server refuses
  every message (a blocked account, a quota, a policy). Each refusal still
  sends its message to the failed queue. But the yuno drops the session as
  for a failure. The next message waits the paced delay (a delay that grows
  after each failure in a row, see below). The yuno logs
  the run once, *"SMTP server refused the last messages in a row: each goes to
  the failed queue, the next ones are paced"*, with `refused_in_row`. There is
  no ERROR of `timeout_failing_alarm` for it. The emails are not stuck: the
  ERROR *"moved to failed queue"* of each one says it.
  Every refusal counts, a bad address included (`550 5.1.1` no such user, a
  sender of the message's own). Some queues are refused one by one. Examples
  are a server that takes no foreign sender, and a batch to a dead address.
  Another example is a server that refuses `RSET` and gets `QUIT` each time.
  Such a queue must not get one
  login per message. The cost: a good email behind a run of refused ones
  waits the paced delay, until the next delivery ends the run. So a queue of
  messages that the server refuses one by one costs one attempt per message,
  at the paced rate. With `timeout_retry` 1 s, 20 such messages see four
  connections in the first 10 s or so, not 20 in a second. Up to 7.25.20 a
  `5xx` to RCPT TO or DATA dropped the session, and the next message logged
  in again at once.

  A `5xx` to ONE recipient of several refuses that recipient only. The
  message goes to the others (RFC 5321 §3.3), and each refused recipient is a
  WARNING (*"RCPT TO rejected"*, with the reply). The *"email sent"* line
  then says who got it and who did not. `to` and `cc` hold only the accepted
  addresses, and `refused` holds the refused `to`/`cc` ones. The bcc go as
  counts (`bcc_count`, `refused_bcc_count`):

  ```json
  {"msg": "email sent", "to": "", "cc": "copy@example.com", "bcc_count": 0,
   "refused": "reader@example.com", "refused_bcc_count": 0, "url": "..."}
  ```

  Up to 7.25.20 one bad address sent the message to nobody, into the failed
  queue.

  A refused sender (MAIL FROM): the REPLY decides, not the `from`. Every
  producer sets the `from` of its emails (the `from` of the yuno is only the
  default). So most emails carry a sender of their own, and an account block
  answers them all in the same way:

  - The refusal belongs to the message only when two conditions are true.
    The sender is the message's own (not the default, and the comparison
    ignores case). AND the reply is about the FORM or the EXISTENCE of that
    address. One such reply is a `501` (its syntax). Another is a `5.1.7`
    status (bad sender mailbox syntax, RFC 3463). The last is a `5.1.8` /
    `553` whose text says that the domain or the address does not exist
    (*not found*, *does not exist*, *unknown*, *no such*, *unroutable*), for
    example `553 5.1.8 <a@b>: Sender address rejected: Domain not found`.
    The message is refused as above: once, to the failed queue, with a
    WARNING that names the `from`. The session continues, and a run of them
    is paced like any run of refusals.
  - The refusal belongs to the account in every other case, whatever the
    `from`. This is true whether or not the reply quotes the address.
    Postfix writes every sender-stage reject as
    `<addr>: Sender address rejected: ...`, the policy ones included. These cases are:
      - any `5.7.x` (a quota,
        `554 5.7.1 <a@b>: Sender address rejected: Access denied`, not
        owned by the user),
      - a `5.1.8` that does not say that the address does not exist
        (`550 5.1.8 Access denied, bad outbound sender`, an account blocked),
      - any `4xx`,
      - any refusal of the default sender.

    It is a failure of the server. The yuno paces it, says it, and charges it to the message as a
    retry. So after `max_retries` paced attempts the message goes to the
    failed queue and the queue moves on. At most one email goes to the failed
    queue per `max_retries` cycle. A stuck head never blocks the queue
    forever, and a head stuck past `timeout_failing_alarm` raises the ERROR.
    The yuno does not stop on it: a quota passes.

  Every attempt that ends in a refusal of the message has one of two
  results, whatever the session was doing. It sends the message to the
  failed queue, or it spends one of its retries.
  Up to 7.25.20 any `5xx` there sent the message to the failed queue at once,
  and every message behind it in turn.

  The email at the head of the queue is the one that every other email waits
  behind. To remove it at once, without its paced attempts, run `skip-email`
  (see Usage). The command moves that email to the failed queue (a WARNING,
  *"email skipped by command: moved to failed queue"*). It answers with the
  `to` and `subject` of the email. The yuno tries the next email at once if
  the pace allows it. The command works while the yuno is paused too (the
  command opens the queues for it).

  A `4xx` to the message (a rate limit, a greylist, a `421`) IS server
  trouble. The session drops, the reconnection is paced, and the code goes
  up on `EV_ON_CLOSE` for a retry.
- Bad content (MIME build or recipient parse failures) → permanent,
  dead-letter. A message that the session cannot send at all (no valid
  recipient, no sender) gets one answer, with `EV_ON_MESSAGE`
  `{ok: false, permanent: true}`. A send that the session does not take
  (`-1`) gets no answer. The message waits at the head of the queue, with no
  retry spent. Up to 7.25.20 the refusal was answered AND returned -1, and
  the message was resolved twice. A loop drains any number of such messages
  in a row (a batch persisted by a broken sender), and the stack stays where
  it is. Up to 7.25.20 the yuno sent each one from inside the resolution of
  the one before, about 1 KB of stack each. Some 9000 of them overflowed an
  8 MB stack.
- Rejected credentials (a `5xx` to `AUTH PLAIN`: `535`, `534`, `530`,
  `538`, ...) → the yuno logs one ERROR, with the reply text of the server.
  Then it exits with code 0 (`LOG_OPT_EXIT_ZERO`). So neither the watcher nor the
  agent relaunches it. Retrying wrong credentials is what gets the address of
  the node banned by the mail provider. `C_SMTP_SESSION` reports it on
  `EV_ON_CLOSE` as `auth_rejected` (the reply code), apart from `code`. The
  server never saw the message, so it stays queued without spending a retry.
- A `334` to `AUTH PLAIN`: we send the credentials with the command, as
  RFC 4954 allows. The RFC also lets the server ask for them on a line of
  their own with a `334`. The session gives that line once, with the same
  credentials. This is still a single login, not a probe. A second `334`
  will repeat at every connection. So the session handles it like rejected
  credentials (`auth_rejected: 334`, the exit says *"SMTP server does not
  take AUTH PLAIN"*).
- A server that refuses this client stops the yuno in the same way. The
  refusal is a `5xx` to the greeting or to EHLO. An example is a `554` of a
  provider that blocked the address. The yuno logs one ERROR (*"SMTP server refuses this
  client: exiting, NOT relaunched"*, with the reply). Every attempt will meet
  the same answer and the provider will see it. `C_SMTP_SESSION` reports it
  on `EV_ON_CLOSE` as `refused` (the code), and the session sends no
  credentials. Before, the yuno retried it, paced, forever (7.25.20: four
  attempts per message, then the failed queue).
- A transient refusal of the login (a `4xx` to `AUTH PLAIN`: `454`
  temporary authentication failure, `421`, `432`, ...) says nothing of the
  credentials. The session closes like any drop, and the login is tried again
  at the next connection, paced. The message in flight spends no retry,
  because the server never saw it. (Up to 7.25.20 it was taken as a refusal,
  and a brief outage of the provider stopped the yuno for good while the
  queue piled up.)
- Retries are paced, every one of them. After a failed session or
  connection, the next connection waits. These failures are:
    - a `4xx` refusal of the login or of the message,
    - the default sender refused,
    - a run of refused messages (above),
    - a `4xx` or a `421` to the end of DATA (a rate limit, a greylist),
    - a reply that never comes, a malformed reply or a reply that is too long,
    - a server that closes the connection by itself (a RST, a TLS error, a
      close with no reply),
    - a connection that is refused, times out, or whose TCP connect or TLS
      handshake does not end within `timeout_response`.

  Up to 7.25.20 nothing watched the TCP connect and the TLS handshake. A
  server that took the connection and stalled TLS kept the message waiting
  forever, with nothing logged. The wait is `timeout_retry` ms (default
  `2000`), twice as long after each further failure in a row, up to
  `timeout_retry_max` (default `600000`). An email queued during the wait
  waits for it too. So does a pause and a play of the yuno: a stop in the
  middle of a failing streak counts as one more failure.

  The doubling starts again from `timeout_retry` after a session that ends
  with no failure (a message delivered, an idle session closed by either
  side). So the yuno does not hold a server that works to the delay of an old
  outage. After such a close, a waiting message goes on a new connection as
  soon as the transport is down. This is also true when the message comes
  while the transport is still closing. The session does not begin it on the
  closing connection. That used to count as a failure of the server and a
  retry of the message. A connection that the session cannot make (refused,
  timed out, a TLS handshake that fails) is a failure like the others. The
  `C_TCP` publishes nothing for it. The session sees it as the
  `EV_STATE_CHANGED` of the `C_TCP` out of `ST_WAIT_CONNECTED`, with the
  cause that the `C_TCP` writes in its `disconnect_cause`.

  A changed url (`set-url-from`, then a pause and a play) starts the pacing
  and the streak again, because they belonged to the old server. Up to
  7.25.20 the corrected server waited the delay of the old one, up to
  `timeout_retry_max`.

  The yuno refuses with an ERROR a `timeout_retry` below 1000 ms, a
  `timeout_retry_max` below `timeout_retry` and a `timeout_response` below
  1000 ms, and takes the default. A `timeout_retry` of 0 reconnected in a
  tight loop after each failed handshake, and a timer of 0 arms nothing.
- Nothing connects with nothing to send. The `C_TCP` never connects by
  itself: not when it starts (`connect_on_start` false), not after a close
  (`timeout_between_connections` -1). Every connection belongs to the
  session, which makes it when it holds a message. A yuno that starts or
  plays with an empty queue logs in to nobody. After any close, nothing logs
  in again until an email comes. One example of such a close is an idle
  session that the server ends (OVH closes them early, with a `421`).
  Another is a failure with nothing queued behind it. Up to 7.25.20 the `C_TCP` connected
  at every start of the session and reconnected 2 s after any close that it
  did not decide. It logged in with nothing to send, forever.
- A failing server is reported once, then loud. The first failure of a
  streak, while an email waits, is a WARNING, *"SMTP server failing: emails
  wait, the retries are paced"*. For a run of refused messages it is
  *"SMTP server refused the last messages in a row"*, which never becomes the
  ERROR. The WARNING has its `cause`: the reply of the server, or *"cannot
  connect: Connection refused"*, which the `C_TCP` itself logs only when
  traced. `timeout_failing_alarm` has the default `3600000` (1 h), and `0`
  means never. When the streak lasts that long, it becomes an ERROR,
  *"SMTP server failing for too long: emails are NOT being sent"*. The yuno says it again at most once
  per that period while it lasts, at the retries, with no timer of its own.

  The first delivery ends a streak with an INFO, *"SMTP server works again:
  emails delivered"* (`failed_s`). A streak of connections and handshakes
  only ends at the next handshake that works, *"SMTP server answers again"*.
  Any streak ends at a session closed with no failure. With no email
  waiting, there is no streak and the yuno says nothing. Up to 7.25.20 a
  server that the yuno did not reach left no trace at the default levels.

  A message spends one of its `max_retries` per failure of ITS transaction.
  The transaction starts at its first RCPT TO: a `4xx`, a close or a timeout
  from there on counts. The default sender refused at MAIL FROM counts too.
  The session tells it with `transaction` on `EV_ON_CLOSE`. A failure before
  the transaction (the connection, the greeting, EHLO, a transient AUTH)
  spends no retry. The server never saw the message, which waits at the head
  of the queue for as long as the outage lasts, paced. Up to 7.25.20 each of
  those failures spent a retry too, 2 s apart. The exception was a
  connection that the session did not make at all. A server that failed its handshake sent
  the message at the head of the queue to the failed queue in about 8 s.
  Then it did the same to the next one, for as long as it failed.

  The exact schedule. The wait after the k-th failure in a row is
  `timeout_retry` × 2^(k-1), capped at `timeout_retry_max`. With the
  defaults, it is 2, 4, 8, 16, 32, 64, 128, 256 and 512 s, then 600 s after
  every further failure. "In a row" counts every failure of the session, of
  any message and of any kind. A failure that spends no retry (a refused
  connection, a `421` greeting) takes the next step too. The first two
  messages in a row refused with a `5xx` take no step and reset nothing.
  From the third on, each refusal is a failure and takes a step.
  The count goes back to the start only after a delivery. A session that
  ends with no failure (an idle session closed by either side) also resets
  it. Moving a
  message to the failed queue does NOT reset it. The next message continues
  from where the count stands. Here is the schedule with the defaults
  (`max_retries` 4), against a server that refuses every message with a
  `4xx`:

  | Message | Wait before its first attempt | Waits between its four attempts |
  |---------|-------------------------------|---------------------------------|
  | the first after a delivery or a start | none | 2 + 4 + 8 = 14 s |
  | the second | 16 s | 32 + 64 + 128 = 224 s |
  | the third | 256 s | 512 + 600 + 600 = 1712 s (28.5 min) |
  | the fourth and later | 600 s | 3 × 600 = 1800 s (30 min) |

  (The connection and the session of each attempt add their own time to
  these waits.)

  A batch configuration for a provider known to refuse for a
  long time raises `max_retries`. With `'max_retries': 10` the waits of the
  first message add up to 2 + 4 + ... + 512 = 1022 s, about 17 minutes. A
  message later in the same streak gets 9 × 600 s = 90 minutes:

  ```json
  "kw": {
      "max_retries": 10,
      "timeout_retry": 2000,
      "timeout_retry_max": 600000
  }
  ```

  Up to 7.25.20 every retry came after the fixed 2 s of the transport, even
  when it failed again and again. The four attempts of a message were gone in
  about 8 s. A `4xx` to the end of DATA sent the message again at once, on
  the same session: four uploads in the same second.
- Why a session closed: a reply of the server causes some closes (a
  refused login, a refused message, a failed EHLO, ...). Each of them
  carries the text of that reply on `EV_ON_CLOSE` as `reply`, next to its
  code.
- Severity: the server can end a session with a refusal, a `4xx`, an
  unexpected or malformed reply, or a reply that never comes. Each is a
  WARNING of the `Protocol` msgset from `C_SMTP_SESSION`, with the reply
  (capped at 512 bytes). An ERROR from `C_SMTP_SESSION` is our own failure (no memory, an
  encoder). The ERRORs of the emailsender are not about who failed. They are
  about what an operator must act on, whoever caused it. The first is
  `email NOT sent, moved to failed queue`: the email will not arrive unless
  somebody sends it again. The second is the exit on a refused login: the
  yuno stops. A retry is a WARNING.
- What the logs name: `to` and `cc`, the `url` of the server that the message
  was sent to, and `bcc_count`. The `url` is the one that the session runs
  on. A `set-url-from` does not change it until the next start. Up to 7.25.20
  the log named the new url. The logs never name the `bcc` addresses, which
  are hidden by definition (the log reaches the logcenter).
- Binary bodies: the yuno persists a non-UTF-8 body as base64 under
  `body_base64` and decodes it at send time. A plain `json_string` will drop
  such a body with no error.

Layering, who reconnects: `c_emailsender` does NOT manage reconnection (no
timers at this level). It only enqueues and dispatches the head message
(`EV_SEND_MESSAGE`) when it is playing, whatever the state of the link. The
bottom `C_TCP` runs with `timeout_inactivity`, which closes the idle SMTP
link. So when the link is down, `c_smtp_session` reconnects on demand. This
is the gclass that owns the transport and must do the SMTP handshake again.
On `EV_SEND_MESSAGE` in `ST_DISCONNECTED`, it kicks its bottom `C_TCP`
(`EV_CONNECT`) when the paced delay is over (before, on its own timer). Then
it runs banner→EHLO→AUTH again, and begins the stashed message on entry to
`ST_IDLE`. The `C_TCP` never reconnects by itself. The pacing belongs to the
session, not to the sender.

On that entry to `ST_IDLE`, the child also publishes `EV_ON_OPEN` *before* it
begins the stashed message. `c_emailsender` moves to `ST_WAIT_RESPONSE`
before it dispatches (`EV_SEND_MESSAGE`). So that `EV_ON_OPEN` lands while
the parent is already in `ST_WAIT_RESPONSE`. For this reason
`ST_WAIT_RESPONSE` accepts `EV_ON_OPEN` as a no-op. The action only marks
the link ready. It does not dequeue again, because a message is already in
flight. The send completes normally and the next `EV_ON_MESSAGE` resolves
it. Without that handler, every reconnect-to-deliver cycle logged a false
*"Event NOT DEFINED in state"*, even when the mail was delivered.

Pause and play. A pause (or the stop of the yuno) with a message in
flight leaves it at the head of `emails_queue`, with no retry spent. The
session drops what it holds and says nothing of the connection that it
closes. The next play sends the queue again. This is also true when the play
comes right after the pause, while the old connection is still closing. In
that case the session starts its `C_TCP` when that close ends. Up to 7.25.20 the pause
freed the message while the emailsender kept pointing at it. A queued
message then waited, after a play, until the yuno sent the next email.

## Build & deploy

Use the standard yuneta build (`make install` from the `build/` directory of
the yuno). Then run the `install-binary` + `create-yuno` + `run-yuno` cycle
of the agent. See
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

Production deployments live in the private repo of each application,
under its `batches/<host>/` tree.
