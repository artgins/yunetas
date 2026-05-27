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

## Build & deploy

Standard yuneta build (`make install` from the yuno's `build/`
directory) followed by the agent's `install-binary` + `create-yuno` +
`run-yuno` cycle. See
[`LIFECYCLE.md`](../yuno_agent/LIFECYCLE.md) §6.

Production deployments live in the private `estadodelaire` repo
under its `batches/<host>/` tree.
