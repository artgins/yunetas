# emailsender

Yuno that sends emails on behalf of other Yuneta services. Uses libcurl for SMTP / HTTP API delivery with configurable handlers and logging.

> Note: `emailsender` links against libcurl and **cannot be built as a fully-static binary** (`CONFIG_FULLY_STATIC`); build it dynamically even when the rest of the suite is static.
