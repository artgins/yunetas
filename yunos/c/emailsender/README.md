# emailsender

Yuno that sends emails on behalf of other Yuneta services. Speaks
native SMTPS (implicit TLS, RFC 8314) on top of `C_SMTP_SESSION` +
`C_TCP`, with its own MIME encoder. Queues outgoing messages with
TimeRanger2 persistence and processes them asynchronously.

> Since 7.4.3 the yuno no longer links libcurl; it builds fully
> static like every other yuno in the suite.

## ⚠️ Operational secret risk — SMTP password in cleartext

Per project memory `project_emailsender_smtp_secret`:

> The SMTP password used by `emailsender^artgins` is currently stored
> **in cleartext** in three places:
>
> 1. The yuno's batch JSON inside the private `estadodelaire` repo.
> 2. The agent's `treedb` configuration node (it gets written on
>    `update-config`).
> 3. The yuno's runtime config materialised into
>    `/yuneta/realms/<owner>/<realm>/emailsender/bin/N-*.json`.
>
> An env-var migration + rotation was planned but **had not been
> applied as of 2026-05-15**. If you touch this yuno before that
> migration ships, treat the credential as compromised.

Mitigations that do not require code changes:

- Rotate the SMTP password in the upstream provider on a schedule.
- Restrict who has read access to `/yuneta/realms/` on production
  hosts (file ownership `yuneta:yuneta`, mode `0750` on the realm
  dirs).
- Never commit production batch JSON containing the cleartext
  secret to a public repo. Even with later rotation, git history is
  forever.

Long-term fix: thread an env var through `c_agent.c` configuration
materialisation so the secret is sourced from the host environment,
not stored in the batch / treedb / config files.

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
- **Binary bodies**: a non-UTF-8 body is persisted base64 under `body_base64`
  (a plain `json_string` would silently drop it) and decoded at send time.

The bottom `C_TCP` runs with `timeout_inactivity` (closes the SMTP link when
idle and does **not** auto-reconnect). So `c_emailsender` drives reconnection
on demand: when mail is queued and the SMTP child is `ST_DISCONNECTED` it sends
`EV_CONNECT` (the session re-runs banner→EHLO→AUTH and drains the queue on
`EV_ON_OPEN`). A `C_TIMER` retries with **exponential backoff** (2s → ×2 → cap
5 min, reset to the minimum on a successful open) so a down server or bad
credentials are not hammered. A **permanent session failure** (AUTH `535`,
EHLO/banner `5xx`) logs a loud `ERROR` and backs off to the cap, but keeps the
mail queued — it flows once the credentials/config are fixed, never silently
lost and never tight-looped.

Inspect the queues at runtime: `ycommand command-yuno id=<id> command=list-queues`
(also `remove-emails-failed` to drain the dead-letter queue).

## Build & deploy

Standard yuneta build (`make install` from the yuno's `build/`
directory) followed by the agent's `install-binary` + `create-yuno` +
`run-yuno` cycle. See
[`YUNO_LIFECYCLE.md`](../yuno_agent/YUNO_LIFECYCLE.md) §6.

Production deployments live in the private `estadodelaire` repo
under its `batches/<host>/` tree.
