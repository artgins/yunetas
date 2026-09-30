# webstats test

Tests of the `webstats` yuno's gclasses (`yunos/c/webstats/src/c_webstats.c`
and `c_log_reader.c`, compiled into the test). The pure helpers of webstats
(`ip_literals.c`, `local_day.c`) are tested in `tests/c/helpers`.

## report_ready

The report a run publishes (`EV_REPORT_READY`) is the report of the day.

1. The access log holds 3 requests of 2026-09-01: `report-day` stores the day
   and publishes it (3 requests).
2. The access log is emptied, as when the day has rotated away, and
   `report-day` runs again: it reads nothing, keeps the stored day (the
   warning *"Read nothing for a day already stored with data, keeping the
   stored one"*) and must publish THAT one, 3 requests. Up to 7.25.20 the run
   mailed the stored report and published the empty one it had read.

No mail is sent (`send_email: false`, `send=0`), no registry is asked
(`whois_enabled: false`); the logs and the store live under
`/tmp/test_webstats_report_ready`, rebuilt at each run.
