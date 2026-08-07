# webstats — design

Status: **working**. It reads the logs of a day, makes the full record of
section 7, keeps it in TimeRanger2, compares the day against its own history
and mails the result as HTML.

Yuno that reads the logs of the node's web server (nginx or openresty), makes
a daily report of traffic, errors and probes, and sends the report by email
through `emailsender`.

One yuno per node. Each node reports on its own logs and sends its own email.

## 1. Why a yuno and not goaccess

`goaccess` gives the traffic tables with one command. It does not give what
this yuno is for:

- **A sealed node has no SSH.** After `NODE_SEALING.md` the only way to look at
  a node is from inside it, outward. A tool you must log in to run is a task
  that stops working the day the node is sealed.
- The daily numbers stay in TimeRanger2, so the report can say what **changed**,
  not only what happened.
- The report leaves the node through `emailsender`, the mail path the suite
  already owns.
- The agent can ask the yuno for any past day with a command.

The suite already ships every binary in `outputs/yunos/` in the `.deb` and the
`.rpm`, so a new yuno here reaches the five nodes with no change to packaging.

## 2. What it does not do

- It does not ban. `fail2ban` bans, with the `yuneta-nginx-probe` filter that
  the packages install. This yuno counts probes and names the top clients.
- It does not keep the raw log lines. The rotated `.gz` files are the archive
  for 30 days. Only the daily aggregates go to TimeRanger2.
- It does not speak SMTP. `emailsender` does.
- It does not follow the log in real time. See §4.

## 3. Where the logs are

The web server writes to one of these two trees. A node runs nginx **or**
openresty. The choice is in `/etc/yuneta/webserver`. The other tree can be
installed and unused.

```
/yuneta/bin/nginx/logs/access.log            /yuneta/bin/nginx/logs/error.log
/yuneta/bin/openresty/nginx/logs/access.log  /yuneta/bin/openresty/nginx/logs/error.log
```

`logrotate` rotates them daily, keeps 30, compresses with `delaycompress`. So
the file of the previous day is `access.log.1` and it is **not** compressed.
The normal daily path never needs gzip. There is no zlib in `outputs_ext`, so
a backfill older than two days is out of scope for phase 1 (§13).

### 3.1 Three generations of line format

The log format was extended twice. All three generations are in the 30 days of
rotated files, and two of them are in the same file until the next rotation.

| Generation | Fields | Quotes |
|---|---|---|
| combined | the stock nginx format | 6 |
| `+ $host` (2026-08-05) | `$host` at the end | 8 |
| `+ times` (this change) | `$request_time`, `$upstream_response_time` | 10 |

Each generation only **appends**. Everything before stays byte for byte the
same, so the field positions that `awk`, goaccess and the factory fail2ban
filters use keep working.

⚠️ **The parser must be tolerant, not strict.** A parser that demands 10 quotes
drops every line written before the change, and every line of a node whose
config has not been updated yet. It must read what is there and leave the rest
unset.

⚠️ **Count the quotes on a copy.** In awk, `n=gsub(/"/,"x")` modifies `$0` and
the later `match` finds nothing. The same trap applies to any in-place scan.

## 4. The line sets the day window, not the file

This is the decision that keeps the yuno small.

The yuno does **not** keep a read offset, and it does **not** hook into
`postrotate`. To make the report of day D it opens `access.log` and
`access.log.1`, reads the `[$time_local]` of every line, and keeps the lines
whose time is inside `[D 00:00, D+1 00:00)` in the offset the line itself
carries.

What this buys:

- **No state.** No offset file, no inode tracking, nothing to repair.
- **No coupling to logrotate.** `logrotate.timer` fires when systemd wants,
  `delaycompress` moves the compression one day, `sharedscripts` makes "which
  file is yesterday" ambiguous. None of it matters.
- **Idempotent.** `report-day date=2026-08-05` can be run again and gives the
  same record.
- **Repairable.** A yuno that was down does not lose the day, as long as the
  file is still there.

The cost is reading two files instead of one. Measured on the five nodes, a
day is 1.5 MB to 3.5 MB. It is not a cost.

## 5. Architecture

```
webstats (yuno)
└── webstats  C_WEBSTATS       service, default_service
    ├── timer     C_TIMER      the daily schedule
    └── reader_N  C_LOG_READER one per file being read (created, used, destroyed)
```

### 5.1 C_WEBSTATS (service)

Owns the schedule, the accumulators, the report and the commands.

States. The lifecycle is states, not flags, so a command that arrives at the
wrong moment fails loudly and names its sender.

| State | Meaning |
|---|---|
| `ST_IDLE` | waiting for the next scheduled run |
| `ST_READING` | one or more readers are feeding lines |
| `ST_REPORTING` | building the report and handing it to `emailsender` |

Events in: `EV_TIMEOUT` (schedule), `EV_LOG_LINES`, `EV_LOG_EOF`,
`EV_LOG_ERROR` (from the readers).
Events out: `EV_REPORT_READY` (`EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS`) so a
future consumer — controlcenter, a SPA view — can take the report without
touching this yuno.

Work starts in `mt_play`, not in `mt_create`. `mt_create` creates the timer.

### 5.2 C_LOG_READER (child)

Turns one file into events. It knows nothing about nginx.

- Opens the file and reads it in bounded chunks, one chunk one action, so the
  loop keeps breathing. A single read of a multi-megabyte file inside one
  action blocks the loop and hides the work from the `machine` trace.
- ⚠️ **io_uring is not used for this, and cannot be yet.**
  `yev_create_read_event()` submits its reads with **offset 0**, which is what
  a socket wants and is wrong for a regular file: every chunk reads the head
  of the file again. No gclass in the tree reads a regular file this way,
  so nothing had shown it. The reader uses `read(2)` and continues on a 1 ms
  `C_TIMER0`. Giving yev a read offset is the proper fix, and it is a kernel
  change that needs its own decision. The events this gclass publishes do not
  depend on which of the two it uses.
- Splits chunks into lines and publishes `EV_LOG_LINES` with an array of
  complete lines.
- ⚠️ **A line crosses the chunk boundary.** The tail of a chunk that has no
  newline is kept and prefixed to the next chunk. This is the classic bug of
  every chunked reader: it eats one line per chunk, which is a small and stable
  percentage, so the numbers look plausible and are wrong.
- Ends with `EV_LOG_EOF` (with the line count) or `EV_LOG_ERROR`.
- CHILD subscription model: it publishes to its parent, which declares both
  events in its FSM.

### 5.3 Why a timer here is not polling

The framework forbids a timer that re-issues the same query to see if
something changed. This timer is a **schedule**: it fires once a day because
the report is defined per day, and no producer can publish the event instead.
The code carries this comment, so nobody removes it later as a discarded
pattern.

## 6. Configuration

Attributes of `C_WEBSTATS`, all settable from the batch config.

| Attribute | Type | Default | Meaning |
|---|---|---|---|
| `access_log_paths` | list | both trees | files to read for the access log |
| `error_log_paths` | list | both trees | files to read for the error log |
| `report_hour` | int | 6 | local hour of the daily run |
| `report_minute` | int | 0 | minute of the daily run |
| `send_email` | bool | true | false leaves the record and skips the mail |
| `email_to` | str | — | destination |
| `email_service` | str | `emailsender` | service that sends |
| `top_n` | int | 20 | rows per top table |
| `max_distinct_keys` | int | 200000 | cap per counter map (§8.3) |
| `probe_patterns` | list | mirrors the fail2ban filter | what counts as a probe |
| `internal_networks` | list | — | prefixes not counted as clients |
| `keep_days` | int | 400 | days of aggregates kept |

`report_hour` defaults to 06:00 and not to 00:05 on purpose: the window comes
from the timestamps, so there is no reason to race `logrotate`, and a report
that is built at six is in the mailbox when somebody opens it.

`internal_networks` matters for the numbers: on `artgins` the clients that
hammered Keycloak were **our own nodes**. Without this list the "top clients"
table is the suite talking to itself.

## 7. What it computes

### 7.1 Access log

- Totals: requests, bytes, distinct clients, status classes 2xx/3xx/4xx/5xx.
- Per hour: 24 counters.
- **Per vhost** (`$host`): requests, bytes, status classes and its own latency
  histogram. This is what `$host` was added for.
  The mail shows the busiest `top_n` and says how many more answered. The name
  comes from the Host header, so anybody can invent one: the first mail from
  e.com listed 52 rows, half of them forged names with one request each.
  Distinct clients are counted **once, globally**, and not per vhost: a set of
  addresses per name multiplies the worst case by the number of vhosts, and
  the number that gets read is the total.
  A line of the first generation has no `$host`, so it lands under `-`. That
  name is the mark of a node whose config is not updated yet.
- **Every 5xx in full**, with time, vhost, request, status and client. Not a
  counter — the lines. Capped, and the cap is reported (§8.3).
- Top paths, top 404 paths, top referrers, top user agents, top clients.
  The 404 table separates real broken links from probes, which are counted
  apart.
- Probes: requests that match `probe_patterns` (any `.php`, `.env`, `.git`,
  `.aws`, `.ssh`, `.svn`, `.hg`, `/wp-*`), with the count, the distinct
  clients and the top offenders.
  ⚠️ The filter must **not** key on the status code. A SPA vhost
  (`try_files … /index.html`) answers **200** to any path. That is the bug that
  made the packaged fail2ban filter blind on both console vhosts.
- **Latency**, once the log format carries it (§10): count, sum, max and a
  fixed histogram, per vhost and total.
  Buckets, in seconds:
  `0.005 0.01 0.025 0.05 0.1 0.25 0.5 1 2.5 5 10 +inf`.
  A histogram and not a list of samples: bounded memory whatever the traffic,
  and the buckets of two days can be added, so the weekly view is free.
  p50/p95/p99 are the **upper edge of the bucket** the percentile falls in,
  not an interpolation. "p95 is at most 0.5 s" is true. A number invented
  between two edges is not. A percentile above the last edge is reported as
  `-1`, which reads as *slower than 10 s*.
  ⚠️ The bucket index is a **ceiling** division. Truncating puts the p95 of
  two measures on the first bucket, so a day where one of two requests took
  half a second reports as a fast day.

### 7.2 Error log

This half is where the real findings were. On 2026-08-05 the error logs held
the Keycloak connection refusals, the `client_max_body_size` hits of real
users, and 185 `bind() Address already in use` on `central` from an automation
that believes it starts nginx and fails silently. None of it was in the access
log.

Lines are grouped by **signature**: the message with its variable parts
removed — `pid#tid`, `*connection`, `client:`, `server:`, `request:`,
`upstream:`, `host:`, and any number. For each signature the record keeps the
count, the first and last time, and one full sample line.

⚠️ `error_page 404 /404.html` pointing at a file that does not exist writes
**one** error line, the one for the failing `404.html`, and that line names the
original request. It is one event, not two. Do not count it as two.

## 8. Persistence

TimeRanger2, one topic.

- Topic `daily_stats`, pkey `date` (`YYYY-MM-DD`), `sf_string_key`, one record
  per day, a few kilobytes each. `keep_days` bounds it: at every play and at
  the end of every run, the keys that sort before `today - keep_days` are
  dropped. An ISO date sorts the same as it reads, which is why the key has
  that shape.

Raw lines are never stored.

**A day reported twice has two records under the same key**, and the newest
one is the answer. That is what makes `report-day` repeatable with no delete
first: a day rebuilt after a fix wins.

⚠️ **Reading the newest is not `(from_rowid=1, limit=1, backward=TRUE)`.**
`from_rowid` is a position among the rows the iterator returns, and `backward`
does not turn it into a position from the end, so that call hands back row 1 —
the **oldest**. Ask `tranger2_iterator_size()` for the row count and read that
rowid forward. With the wrong call `get-report` answered for ever with the
first version of a day, and a re-run to correct a day changed nothing that
anybody can read. Found by rebuilding one day twice and looking at the store.

### 8.4 What changed

The record carries a `changed` block: the previous day's headline numbers, the
median of the last seven days, and today's. That is the whole reason to keep a
history — a number with nothing to compare it to carries no information.

A day with no history says `days_of_history: 0` instead of comparing against
zero. Against zero, the first morning reads as *everything doubled*.

### 8.1 Record shape (version 1)

```json
{
    "date": "2026-08-05",
    "version": 1,
    "node": "wattyzer",
    "generated_at": 1754400000,
    "sources": [
        {"file": ".../access.log", "lines": 14321, "kept": 9812,
         "unparsed": 3, "bytes": 3500000, "too_long": 0}
    ],
    "totals": {"requests": 0, "bytes": 0, "clients": 0,
               "status": {"2xx": 0, "3xx": 0, "4xx": 0, "5xx": 0}},
    "by_hour": [0],
    "by_vhost": {"doc.yuneta.io": {"requests": 0, "bytes": 0, "status": {},
                                   "latency": {}, "latency_summary": {}}},
    "latency": {"count": 0, "sum": 0.0, "max": 0.0, "buckets": [0]},
    "latency_summary": {"avg": 0.0, "p50": 0.0, "p95": 0.0, "p99": 0.0},
    "top": {"paths": [], "not_found": [], "referrers": [], "agents": [], "clients": []},
    "server_errors": [{"host": "", "client": "", "status": 500, "path": "", "hour": 0}],
    "probes": {"requests": 0, "clients": 0, "top_patterns": [], "top_clients": []},
    "errors": {"total": 0, "distinct": 0, "by_signature": [
        {"signature": "", "count": 0, "first": "", "last": "", "sample": ""}
    ]},
    "truncated": [{"counter": "paths", "dropped": 0}]
}
```

Every `top` row is `{"key": …, "count": …}`. `latency` keeps the raw buckets
so two days can be added. `latency_summary` is what a reader looks at.
`sources[].lines` is what the file holds, `kept` is what fell inside the day,
and `unparsed` is what the parser did not understand.

`version` is in the record because the shape will grow. A reader that finds a
version it does not know says so. It does not guess.

### 8.2 Nothing is silently dropped

- `sources[].unparsed` counts the lines the parser did not understand, per
  file. A parser that quietly skips what it cannot read reports a quiet day.
- `truncated` lists every cap that was hit, with the counter and the number of
  keys dropped. A cap that is not reported reads as full coverage.

### 8.3 Caps

Counter maps are capped at `max_distinct_keys`. A day under attack can produce
a very large number of distinct paths, and an unbounded map is how a report
generator becomes the incident. On reaching the cap the map stops accepting new
keys, keeps counting the ones it has, and adds an entry to `truncated`.

## 9. The report

The report is a mail that has to be **read**, every day, by somebody who is
busy. So it is short and it is ordered by what needs a decision.

1. **Needs attention.** 5xx by vhost, new error signatures that were not in the
   previous days, latency above the previous week, a source file that the yuno
   cannot read, zero lines parsed.
2. **What changed.** Requests, bytes, 4xx, 5xx, probes, p95 — each with the
   previous day and the median of the last 7 days, read from `daily_stats`. A
   number with no comparison carries no information.
3. **Traffic.** Per vhost, per hour, the top tables.
4. **Errors.** Signatures with counts and one sample.
5. **Probes.** Count, clients, top offenders.

Body is HTML, self contained: no image and no external stylesheet, and the
styles are inline because a mail client throws a `<style>` block away.

⚠️ **The body must be broken into lines.** SMTP allows at most 1000 octets per
line (RFC 5321), and this report is one single line of 40 KB. A relay is
entitled to fold it, and OVH does: it inserts CRLF+space every ~1000 bytes, in
the middle of whatever is there. The first mail from e.com arrived with
`&Delta;` split into `& Delta;`, a row label reading `cl ients`, a path
reading `ac cess.log`, and a `<td` cut in half — which stops being a tag and
prints as text in the middle of a table. A newline goes between every pair of
adjacent tags: it can never split a word, because the text of the report holds
no raw `>` after escaping, and HTML collapses the whitespace. The
hour histogram is drawn with `div` widths, so the mail asks no server for
anything when it is opened. `emailsender` builds the MIME part from `body`
with `is_html`.

⚠️ **Every string that came out of the log is HTML-escaped**, and that is not
decoration. The path, the referer and the user agent are written by whoever
made the request. Without escaping, a probe for `/<img src=x onerror=…>`
arrives as live markup inside our own mailbox — the scanned node scripting the
reader of the report about the scan. Verified by putting exactly that request
and a `<script>` user agent through a real run: both come out as `&lt;img` and
`&lt;script`.

**The subject carries the day's verdict**, never the same text twice in a row:
`NO DATA`, or `N 5xx, M requests`, or `M requests, N errors`. A subject that
reads the same every morning trains the reader to leave it unopened, which is
the end of a daily report.

⚠️ **No attachment by default.** `emailsender` reads `attachment` as a path in
**its own** process. If the node's `emailsender` is reached through the agent
on another node, the path does not exist there and the attachment is lost or
wrong. The full JSON stays in TimeRanger2 and is fetched with `get-report`.

### 9.1 Silence is a failure

If the yuno parses zero lines for the day, it **still sends the mail**, and the
subject says so. A report generator that says nothing when it is broken is
indistinguishable from a quiet day, and that is exactly how a fail2ban jail sat
healthy for months without banning anybody. The rule from that session applies
here: **a control that declares itself healthy is not verified.**

For the same reason `list-sources` exists (§11): it says which files it will
read, whether each one exists and whether it can be read, and it is run after
install instead of assuming.

### 9.2 Delivery

`required_services: ['emailsender']` in the yuno config, then:

```c
hgobj gobj_emailsender = gobj_find_service("emailsender", FALSE);
gobj_send_event(gobj_emailsender, EV_SEND_EMAIL, kw_email, gobj);
```

`kw_email` carries `to`, `subject`, `body`, `is_html`. This is the path
`logcenter` already uses. The body goes as a string, not as a `gbuffer`:
`emailsender` normalizes a `gbuffer` into `body` anyway, and a string does not
touch the kw auto-decref trap.

Failure to reach `emailsender` is logged with the report kept in
`daily_stats`, so the mail can be re-sent with `report-day date=… send=1`.

## 10. The nginx side

`$request_time` and `$upstream_response_time` are added at the **end**, with
the same discipline as `$host`: everything before stays byte for byte the same.

```nginx
log_format vhost '$remote_addr - $remote_user [$time_local] "$request" '
                 '$status $body_bytes_sent "$http_referer" "$http_user_agent" '
                 '"$host" $request_time "$upstream_response_time"';
```

- `$upstream_response_time` is quoted: it is `-` with no upstream, and a list
  (`0.001, 0.002`, or `0.001 : 0.002` with an internal redirect) with more than
  one. Unquoted it breaks the field count.
- `$request_time` is a plain number and needs no quotes.
- Quote count goes 6 → 8 → 10, so the generation of a line stays readable at a
  glance and in the parser.

**Rollout.** The configs are not in this repo. Each one is owned by an
operations repo and is uploaded by its own `deploy-conf.sh`
(see `reference_nginx_config_ownership`): `artgins` and the `yuneta.io` root in
the artgins ops repo on branch `master`, `app.wattyzer.com` in wattyzer,
`demo.estadodelaire.com` in estadodelaire, and one per yunovatios node. Patch
the repo and the node in the same session, `diff` them, `nginx -t` **before**
reload, and on `artgins` take a `curl` picture of the other domains before and
after — a reload there also touches Keycloak, code, doc and www.

Until a node is updated, its lines have 8 quotes and the latency block of that
node's report is empty. That is expected and the report says it rather than
showing zeros.

## 11. Commands

| Command | What it does |
|---|---|
| `analyze-now` | build the report for yesterday, now |
| `report-day report_date=YYYY-MM-DD [send=1]` | rebuild any day still on disk. Sends only with `send=1` |
| `get-report report_date=YYYY-MM-DD` | the stored aggregate record, newest version of that day |
| `list-reports` | the dates held in `daily_stats` |
| `list-sources` | the files it will read, and whether each is readable now |
| `preview-report report_date=YYYY-MM-DD` | the mail body of a stored day, as HTML, without sending it |

⚠️ **The day parameter is `report_date`, not `date`.** `command-yuno` hands its
WHOLE kw to the node query that picks the yuno, so a parameter named like a
field of the yuno record becomes a filter on that field. The yuno record has a
`date` (when it was created), so `command-yuno id=91 command=report-day
date=2026-08-05` matches no yuno and answers **"Yuno not found"** — naming the
yuno, never the parameter. This is the documented `id=` collision, and it is
not limited to `id`: check the columns of `list-yunos` before naming a
parameter. The handlers still read a plain `date` as a fallback, for a caller
that reaches the gclass directly.

## 12. Trace levels

| Level | Shows |
|---|---|
| `read` | file opened, chunks, lines, EOF |
| `parse` | lines the parser rejected, with the line |
| `report` | the built report before it is handed to `emailsender` |

`parse` is the one that matters: it is the only way to see a format change that
the parser silently tolerates.

## 13. Phases

**Done.** The pipeline and every aggregate of section 7: the tolerant parser
of the three generations, totals, per hour, per vhost, the tops, all 5xx
whole, the probes, the latency histogram with its percentiles, and the error
signatures. Caps and unparsed lines are reported in the record.

Verified against a log of ten lines built to hit each one: the three
generations of format, a query string that must be cut off the path, a probe
answered with **200** on a SPA vhost, a client of `internal_networks`, a
malformed line, lines of two other days, and two `connect() failed` lines with
different pid, connection and client that have to fold into one signature.
It counted 8 requests of 9 kept from 10 lines, 5 distinct clients of 6, the
2xx/4xx/5xx split, 2 probes from 2 clients, and 2 signatures from 3 error
lines. No error, no warning, no leak on shutdown.

**Store: done.** The `daily_stats` topic, the pruning by `keep_days`, the
`changed` block against the previous day and the median of the week, and
`get-report` / `list-reports` on top of it.

Verified under a real agent on the dev node, against the 126861 lines of its
own access.log: three days built with `report-day`, listed by `list-reports`,
read back by `get-report`, and one day rebuilt a second time to prove the
newest version wins and that the rebuild finds its two days of history.
Orderly shutdown with no error and no leak.

**Report: done.** The HTML body of section 9, ordered by what needs a
decision, and the subject that says the day's verdict. `preview-report` shows
the body of a stored day without sending anything, so a change to the report
can be looked at before it reaches a mailbox.

`report-day` **refuses a day older than `keep_days`** instead of reading the
logs and writing a record that the pruning of that same run then drops. Doing
the work and throwing it away is worse than saying no: the command answered
*"reading the logs"*, the mail arrived, and `get-report` then said the day did
not exist. Found by asking for a day from January.

**Phase 1 is complete.** What is left is phase 2 (§13 below).

**Not done, by design:** the mail carries no attachment (§9), and the store
keeps only the top of each counter, so *"new error signature"* means *not
among yesterday's top signatures*.

**Phase 2**

- gzip, to rebuild any of the 30 days on disk. Needs zlib in `outputs_ext`.
- Cross with fail2ban: probes seen against IPs banned, read from
  `/var/log/fail2ban.log`. The lesson of that session was that a jail can be
  healthy and ban nobody. This is where that becomes visible.
- One mail for the five nodes, assembled in controlcenter, instead of five.
  Five daily mails is a fast route to nobody reading any of them.
- Immediate alarm on a 5xx burst, instead of waiting for the daily mail.
- A view in the agent SPA over `get-report`.

## 14. Open questions

- Retention of `keep_days=400` is chosen for year-over-year comparison. It is a
  guess until we see the record size on a busy node.
- The probe pattern list is duplicated here and in the fail2ban filter that the
  packagers write. They will drift. Worth a single source in `tools/` later.
