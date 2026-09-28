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
the SMTP session, starts it, and the queue is sent. (Before this, a blank
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
  code is carried up from `C_SMTP_SESSION` (via `EV_ON_CLOSE` for mid-transaction
  drops, `EV_ON_MESSAGE` for the DATA ack); `code in [500,600)` ⇒ permanent.
- **Bad content** (MIME build / recipient parse failures) → permanent, dead-letter.
- **Rejected credentials** (any reply but `235` to `AUTH PLAIN`) → the yuno logs
  one ERROR and **exits with code 0** (`LOG_OPT_EXIT_ZERO`), so neither the
  watcher nor the agent relaunches it: retrying wrong credentials is what gets the
  node's address banned by the mail provider. `C_SMTP_SESSION` reports it on
  `EV_ON_CLOSE` as `auth_rejected` (the reply code), apart from `code`: the
  message was never seen by the server, so it stays queued without spending a
  retry.
- **Binary bodies**: a non-UTF-8 body is persisted base64 under `body_base64`
  (a plain `json_string` would silently drop it) and decoded at send time.

**Layering — who reconnects:** `c_emailsender` does NOT manage reconnection (no
timers up here). It just enqueues and dispatches the head message
(`EV_SEND_MESSAGE`) whenever it is playing, regardless of link state. The bottom
`C_TCP` runs with `timeout_inactivity` (closes the idle SMTP link), so when the
link is down it is **`c_smtp_session`** — the gclass that owns the transport and
must redo the SMTP handshake — that reconnects on demand: on `EV_SEND_MESSAGE`
in `ST_DISCONNECTED` it kicks its bottom `C_TCP` (`EV_CONNECT`), re-runs
banner→EHLO→AUTH, and begins the stashed message on entry to `ST_IDLE`. Retry
pacing for a down server is the C_TCP layer's concern, not the sender's.

On that entry to `ST_IDLE` the child also publishes `EV_ON_OPEN` *before* it
begins the stashed message. Because `c_emailsender` moves to `ST_WAIT_RESPONSE`
**before** dispatching (`EV_SEND_MESSAGE`), that `EV_ON_OPEN` lands while the
parent is already in `ST_WAIT_RESPONSE` — so `ST_WAIT_RESPONSE` accepts
`EV_ON_OPEN` as a no-op (just marks the link ready; it does **not** re-dequeue,
a message is already in flight). The send completes normally and the following
`EV_ON_MESSAGE` resolves it. Omitting that handler made every
reconnect-to-deliver cycle log a spurious *"Event NOT DEFINED in state"* even
though the mail was delivered.

Inspect the queues at runtime: `ycommand command-yuno id=<id> command=list-queues`
(also `remove-emails-failed` to drain the dead-letter queue).

## Build & deploy

Standard yuneta build (`make install` from the yuno's `build/`
directory) followed by the agent's `install-binary` + `create-yuno` +
`run-yuno` cycle. See
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

Production deployments live in the private `estadodelaire` repo
under its `batches/<host>/` tree.
