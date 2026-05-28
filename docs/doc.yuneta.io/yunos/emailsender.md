(yuno-emailsender)=
# `emailsender`

E-mail sending yuno — speaks native SMTPS (implicit TLS, RFC 8314) on top of
`C_SMTP_SESSION` + `C_TCP`, with its own MIME encoder. Queues outgoing messages
with TimeRanger2 persistence. Builds fully static since 7.4.3.
