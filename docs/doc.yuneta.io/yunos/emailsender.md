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
  MIME message ([`mime_encoder.c`](https://github.com/artgins/yunetas/blob/7.25.20/yunos/c/emailsender/src/mime_encoder.c)) and drives the send/retry loop.
- `C_SMTP_SESSION` implements the line-based SMTP client as an FSM. It uses
  **AUTH PLAIN**. **STARTTLS is not implemented** (the transport is TLS from the
  first byte via the `smtps://` C_TCP bottom). EHLO advertises the local
  hostname. An idle session closed by the server (SMTP 421 — OVH does this
  aggressively) is treated as a graceful close. C_TCP reconnects on the next
  send.

## Configuration

Effective config is the usual merge of `main.c` fixed/variable config with the
external JSON. Inspect it at runtime with
`ycommand command-yuno id=<id> service=__yuno__ command=view-config`.

| Attribute | Default | Purpose |
|-----------|---------|---------|
| `url` | *(required)* | SMTP server, for example `smtps://ssl0.ovh.net:465` |
| `from` | *(required)* | Default envelope/From address |
| `from_beautiful` | `""` | Optional display name for the From header |
| `username` / `password` | `""` | AUTH PLAIN credentials. Blank → the SMTP side does not start; set them with `set-email-user` |

`url`, `from`, `username` and `password` are persistent attributes: set them at
runtime with the `set-email-user` / `set-url-from` commands (below) and they are
saved to the yuno's persistent-attrs store and reloaded on restart, over the
config values. This is the canonical way to provision the SMTP credentials: a
batch config leaves `username` and `password` blank, and they are set once with
`set-email-user`.

While either one is blank the SMTP side does not start. The yuno runs, accepts
and queues emails, and logs one ERROR (*"SMTP username or password is empty:
emails are queued, NOT sent. Set them with the set-email-user command"*). The
command applies at once, with no restart: the session starts and the queue is
sent.

```bash
ycommand -c 'command-yuno id=<id> service=emailsender command=set-email-user username=no-reply@example.com password=<password>'
```
| `max_retries` | `4` | Max total send attempts before dead-lettering |
| `timeout_retry` | `2000` | ms before the connection that follows a failed session or connection; doubles per failure in a row |
| `timeout_retry_max` | `600000` | Cap of that doubling (ms) |
| `timeout_failing_alarm` | `3600000` | ms a failing server may keep emails waiting before it is an ERROR (said again at most once per this period); `0`: never |
| `timeout_response` | `30000` | ms the server has to answer each command |
| `timeout_inactivity` | `30000` | ms of silence after which the idle SMTP connection is closed; the next email opens it again |
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
| `send-email` | `to`, `subject`, `body`, `reply-to`, `attachment`, `inline_file_id`, `is_html` | Enqueue an email. `to`/`cc`/`bcc` accept comma- **or** semicolon-separated lists. Recipients are deduplicated. |
| `list-queues` | — | Dump the messages in `emails_queue` and `emails_failed` with totals. Works while paused (queues are opened temporarily). |
| `remove-emails-failed` | — | Purge the `emails_failed` dead-letter queue. Works while paused. |
| `set-email-user` | `username`, `password`, `url`, `from` | Set the AUTH PLAIN credentials (required) and optionally the SMTP url / default From. All saved as persistent attrs. |
| `set-url-from` | `url`, `from` | Set the SMTP url or both the default From and save them as persistent attrs (at least one required). |
| `enable-alarm-emails` | — | Re-enable alarm emails |
| `disable-alarm-emails` | — | Suppress "ALERT Queuing" alarm emails |
| `help` | `cmd`, `level` | Command help |

`set-email-user` and `set-url-from` are tagged `SDF_AUTHZ_X` — they require the
`__execute_command__` permission when the per-command authz gate is enabled.

Other yunos send mail by publishing `EV_SEND_EMAIL` to the `emailsender`
service (for example `logcenter`'s summary report).

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
  authenticated. If the server is down or the URL is wrong, the message stays
  in the queue. [`C_TCP`](#gclass-c-tcp) keeps reconnecting on its own and delivery resumes
  once the link is back — nothing is dead-lettered just because the link is
  momentarily unavailable.
- **Rejected credentials stop the yuno instead.** When the server refuses the
  `AUTH PLAIN`, the emailsender logs one ERROR (*"SMTP credentials rejected:
  exiting, NOT relaunched"*, with the reply `code`, `url` and `username`) and
  exits with code 0, so neither the watcher nor the agent relaunches it. A retry
  would only repeat the refusal, and a mail provider bans the address that keeps
  failing its AUTH: OVH did, for its whole mail cluster. The in-flight message
  stays at the head of `emails_queue`, without spending a retry. Fix the
  credentials (`set-email-user`, or the config) and run the yuno again:

  ```bash
  ycommand -c 'command-yuno id=<id> service=emailsender command=set-email-user username=<user> password=<password>'
  ycommand -c 'run-yuno id=<id>'
  ```

  The agent runs enabled yunos again when it restarts and on
  `deactivate-snap`, so with the credentials still wrong each of those costs
  exactly one more attempt, never a loop.

  A `334` to `AUTH PLAIN` (the server wants the response on a line of its
  own, RFC 4954) is answered once with the same credentials; a second `334`
  stops the yuno the same way (*"SMTP server does not take AUTH PLAIN"*). So
  does a `5xx` to the greeting or to EHLO (*"SMTP server refuses this client"*,
  a provider that blocked the address): no credentials are sent, and every
  attempt would be refused.
- The body is persisted as part of the queued message (a string), so it
  survives both retries and a yuno restart.
- A pause (or the stop of the yuno) with a message in flight leaves it at the
  head of `emails_queue` without spending a retry; the next play sends the
  queue again, also when it comes right after the pause.
- A message stays at the head of the queue until it is either sent or its
  attempts are exhausted. A transient send failure (server NACK, connection
  dropped mid-send) is retried up to `max_retries` total attempts.
- When `max_retries` is exhausted — or the message is permanently undeliverable
  (missing recipient, un-encodable body) — it is moved to `emails_failed` and
  removed from the main queue. **The dead-letter queue is not retried
  automatically.**

Every outcome is logged: a warning per retry, an error when a message is moved
to the dead-letter queue, and an info line on success.

### Pacing

No failure is retried at once. After a failed session or connection -- a
refused login or message, a `4xx` or `421` to the end of DATA (a rate limit, a
greylist), a reply that never comes, a malformed reply, a server that closes
the connection by itself, a connection that is refused or times out -- the next
connection waits `timeout_retry` ms, twice as long after each further failure
in a row, up to `timeout_retry_max`. An email queued during the wait waits for
it too, and so does a pause and a play. A session that ends with no failure (a
message delivered, an idle session closed) starts the doubling again. Nothing
connects with nothing to send: the transport never reconnects by itself.

A failing server is said: a WARNING at the first failure (*"SMTP server
failing: emails wait, the retries are paced"*, with its `cause`, a refused
connection included), an ERROR once it has failed for `timeout_failing_alarm`
(1 h by default) and again at most once per that period, and an INFO at the
first delivery after it. To be told after 15 minutes instead:

```json
"kw": {
    "timeout_failing_alarm": 900000
}
``` Providers ban the addresses that
hammer them (OVH did, for its whole mail cluster), so this is a hard rule, not
a tuning knob.

A message spends one of its `max_retries` only when it fails in its own
transaction (MAIL FROM onwards). A failure before -- the connection, the
greeting, EHLO, a transient AUTH (`454`) -- spends none: the server never saw
the message, and it waits, paced, for as long as the outage lasts. A batch
config for a provider known to refuse messages for long raises `max_retries`
(with the defaults the four attempts are spread over 2 + 4 + 8 = 14 s;
`max_retries: 10` covers about 17 minutes):

```json
"kw": {
    "max_retries": 10,
    "timeout_retry": 2000,
    "timeout_retry_max": 600000
}
```

## Debugging

Trace levels (enable with
`ycommand command-yuno id=<id> service=__yuno__ command=set-gclass-trace gclass=<G> set=1 level=<L>`):

| GClass | Level | Shows |
|--------|-------|-------|
| `C_EMAILSENDER` | `messages` | The MIME message dispatched to the SMTP child |
| `C_SMTP_SESSION` | `smtp` | SMTP FSM phases — commands sent (`>>>`) and reply codes (`<<<`) |
| `C_SMTP_SESSION` | `traffic` | Raw bytes in/out (hex dump) |
