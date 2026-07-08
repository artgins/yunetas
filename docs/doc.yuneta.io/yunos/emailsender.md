(yuno-emailsender)=
# `emailsender`

E-mail sending yuno. It speaks native **SMTPS** (implicit TLS, RFC 8314) and
queues outgoing messages with TimeRanger2 persistence so they survive transient
SMTP outages and yuno restarts. Builds fully static since 7.4.3.

## Architecture

```
C_EMAILSENDER          <- queueing, retry, dead-letter, MIME encoding
    > C_SMTP_SESSION   <- SMTP client FSM (banner/EHLO/AUTH/MAIL/RCPT/DATA)
        > C_TCP        <- smtps://host:465 (implicit TLS from byte zero)
```

- `C_EMAILSENDER` owns two persistent queues on TimeRanger2: `emails_queue`
  (pending) and `emails_failed` (dead-letter). It builds the full RFC 5322 /
  MIME message ([`mime_encoder.c`](https://github.com/artgins/yunetas/blob/7.7.2/yunos/c/emailsender/src/mime_encoder.c)) and drives the send/retry loop.
- `C_SMTP_SESSION` implements the line-based SMTP client as an FSM. It uses
  **AUTH PLAIN**; **STARTTLS is not implemented** (the transport is TLS from the
  first byte via the `smtps://` C_TCP bottom). EHLO advertises the local
  hostname. An idle session closed by the server (SMTP 421 — OVH does this
  aggressively) is treated as a graceful close; C_TCP reconnects on the next
  send.

## Configuration

Effective config is the usual merge of `main.c` fixed/variable config with the
external JSON; inspect it at runtime with
`ycommand command-yuno id=<id> service=__yuno__ command=view-config`.

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `url` | *(required)* | SMTP server, e.g. `smtps://ssl0.ovh.net:465` |
| `from` | *(required)* | Default envelope/From address |
| `from_beautiful` | `""` | Optional display name for the From header |
| `username` / `password` | `""` | AUTH PLAIN credentials (empty → skip AUTH) |

`url`, `from`, `username` and `password` are persistent attributes: set them at
runtime with the `set-email-user` / `set-url-from` commands (below) and they are
saved to the yuno's persistent-attrs store and reloaded on restart. This is the
canonical way to provision the SMTP credentials — they never have to appear in
any committed config or batch file.
| `timeout_dequeue` | `10` | ms between queue polls |
| `max_retries` | `4` | Max total send attempts before dead-lettering |
| `disable_alarm_emails` | `false` | Drop "ALERT Queuing" alarm emails |
| `tranger_path` / `tranger_database` | | TimeRanger2 store location |
| `topic_emails_queue` | `emails_queue` | Pending-queue topic |
| `topic_emails_failed` | `emails_failed` | Dead-letter topic |
| `backup_queue_size` | `1000000` | Backup the queue topic at this size |

`C_SMTP_SESSION` adds `timeout_response` (default `30000` ms) — the per-command
watchdog while waiting for a server reply.

```{note}
`only_test`, `add_test` and `test_email` attributes exist but are **not wired**
to any logic in the current code; setting them has no effect.
```

## Commands

| Command | Parameters | Description |
|---------|------------|-------------|
| `send-email` | `to`, `subject`, `body`, `reply-to`, `attachment`, `inline_file_id`, `is_html` | Enqueue an email. `to`/`cc`/`bcc` accept comma- **or** semicolon-separated lists; recipients are deduplicated. |
| `list-queues` | — | Dump the messages in `emails_queue` and `emails_failed` with totals. Works while paused (queues are opened temporarily). |
| `remove-emails-failed` | — | Purge the `emails_failed` dead-letter queue. Works while paused. |
| `set-email-user` | `username`, `password`, `url`, `from` | Set the AUTH PLAIN credentials (required) and optionally the SMTP url / default From; all saved as persistent attrs. |
| `set-url-from` | `url`, `from` | Set the SMTP url and/or the default From and save them as persistent attrs (at least one required). |
| `enable-alarm-emails` | — | Re-enable alarm emails |
| `disable-alarm-emails` | — | Suppress "ALERT Queuing" alarm emails |
| `help` | `cmd`, `level` | Command help |

`set-email-user` and `set-url-from` are tagged `SDF_AUTHZ_X` — they require the
`__execute_command__` permission when the per-command authz gate is enabled.

Other yunos send mail by publishing `EV_SEND_EMAIL` to the `emailsender`
service (e.g. `logcenter`'s summary report).

## Delivery semantics

```{figure} ../_static/emailsender_flow.svg
:alt: send-email enqueues to the persistent emails_queue; a message is dispatched only while the SMTP session is connected and authenticated. Success removes it; a transient NACK retries up to max_retries while it stays at the head; a link outage keeps it queued; exhausted or permanently-undeliverable messages move to emails_failed, which is not retried.
:width: 100%

A queued message stays at the head until it is **sent** or its attempts are
**exhausted**; a transient failure retries, a link outage just waits, and only
exhaustion or a permanent error moves it to the `emails_failed` dead-letter.
```

Outgoing messages are held in the persistent `emails_queue` and are never
dropped while waiting:

- A message is only dispatched while the SMTP session is connected and
  authenticated. If the server is down, the URL is wrong, or credentials are
  rejected, the message stays in the queue. [`C_TCP`](#gclass-c-tcp) keeps reconnecting on its
  own and delivery resumes once the link is back — nothing is dead-lettered
  just because the link is momentarily unavailable.
- The body is persisted as part of the queued message (a string), so it
  survives both retries and a yuno restart.
- A message stays at the head of the queue until it is either sent or its
  attempts are exhausted. A transient send failure (server NACK, connection
  dropped mid-send) is retried up to `max_retries` total attempts.
- When `max_retries` is exhausted — or the message is permanently undeliverable
  (missing recipient, un-encodable body) — it is moved to `emails_failed` and
  removed from the main queue. **The dead-letter queue is not retried
  automatically.**

Every outcome is logged: a warning per retry, an error when a message is moved
to the dead-letter queue, and an info line on success.

## Debugging

Trace levels (enable with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<G> set=1 level=<L>`):

| GClass | Level | Shows |
|--------|-------|-------|
| `C_EMAILSENDER` | `messages` | The MIME message dispatched to the SMTP child |
| `C_SMTP_SESSION` | `smtp` | SMTP FSM phases — commands sent (`>>>`) and reply codes (`<<<`) |
| `C_SMTP_SESSION` | `traffic` | Raw bytes in/out (hex dump) |
