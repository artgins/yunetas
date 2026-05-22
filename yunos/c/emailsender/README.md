# emailsender

Yuno that sends emails on behalf of other Yuneta services. Uses
libcurl for SMTP / HTTP API delivery with configurable handlers and
logging.

> **Static-build caveat**: `emailsender` links against libcurl and
> **cannot be built as a fully-static binary**
> (`CONFIG_FULLY_STATIC`). Build it dynamically even when the rest of
> the suite is static.

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

Production deployments live in the private `estadodelaire` /
`hidraulia` repos under their respective `batches/<host>/` trees.
