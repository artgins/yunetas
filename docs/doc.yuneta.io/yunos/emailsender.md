(yuno-emailsender)=
# `emailsender`

E-mail sending yuno — speaks native SMTPS (implicit TLS, RFC 8314) on top of
`C_SMTP_SESSION` + `C_TCP`, with its own MIME encoder. Queues outgoing messages
with TimeRanger2 persistence. Builds fully static since 7.4.3.

## Delivery semantics

Outgoing messages are held in a persistent TimeRanger2 queue (`emails_queue`)
and are never dropped while waiting:

- A message is only dispatched while the SMTP session is connected and
  authenticated. If the server is down, the URL is wrong, or credentials are
  rejected, the message stays in the queue. `C_TCP` keeps reconnecting on its
  own and delivery resumes once the link is back — nothing is dead-lettered
  just because the link is momentarily unavailable.
- A message stays at the head of the queue until it is either sent or its
  attempts are exhausted. A transient send failure (server NACK, connection
  dropped mid-send) is retried up to `max_retries` total attempts.
- When `max_retries` is exhausted — or the message is permanently
  undeliverable (e.g. missing recipient, un-encodable body) — it is moved to a
  separate dead-letter queue (`emails_failed`) and removed from the main queue.
  The dead-letter queue is not retried automatically.

Every outcome is logged: a warning per retry, an error when a message is moved
to the dead-letter queue, and an info line on success.
