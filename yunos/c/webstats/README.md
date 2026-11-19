# webstats: design

Status: working. `webstats` is a yuno that reads the logs of the web server of
its node (nginx or openresty). It makes a daily report of traffic, errors, and
probes, and sends the report by email through `emailsender`.

Run one yuno on each node. Each node reports on its own logs and sends its own
email. To use it:

1. Set `email_to` and `internal_networks` in the batch configuration of the
   yuno (§6).
2. After the install, run the command `list-sources`. Make sure that the yuno
   can read each log file and that no rotation is stalled (§9.1, §11).
3. To get the report of yesterday now, run `analyze-now`. To build the report
   of another day again, run `report-day report_date=YYYY-MM-DD` (§11).

The rest of this document is the design. Each day, the yuno reads the logs of
that day and makes the full record of section 7. It keeps the record in
TimeRanger2, compares the day with its own history, and mails the result as
HTML.

## 1. Why a yuno and not goaccess

`goaccess` gives the traffic tables with one command. It does not give what
this yuno is for:

- A sealed node has no SSH. After `NODE_SEALING.md`, the only way to look at a
  node is from inside it, outward. A tool that you must log in to run is a task
  that stops on the day that the node is sealed.
- The daily numbers stay in TimeRanger2, so the report can tell what changed,
  not only what happened.
- The report leaves the node through `emailsender`, the mail path the suite
  already owns.
- The agent can ask the yuno for any past day with a command.

The suite already ships every binary in `outputs/yunos/` in the `.deb` and the
`.rpm`, so a new yuno here reaches the five nodes with no change to packaging.

## 2. What it does not do

- It does not ban. `fail2ban` bans, with the `yuneta-nginx-probe` filter that
  the packages install. This yuno counts probes, names the top clients, and
  tells which of them fail2ban banned (§7.4).
- It does not keep the raw log lines. The rotated `.gz` files are the archive
  for 30 days. Only the daily aggregates go to TimeRanger2.
- It does not speak SMTP. `emailsender` does.
- It does not follow the log in real time. See §4.
- It does not geolocate. The country and the organization of a client are
  taken from the network registries (§7.3), not from a GeoIP database.

## 3. Where the logs are

The web server writes to one of these two trees. A node runs nginx or
openresty, not both. The choice is in `/etc/yuneta/webserver`. The other tree
can be installed and not used.

```
/yuneta/bin/nginx/logs/access.log            /yuneta/bin/nginx/logs/error.log
/yuneta/bin/openresty/nginx/logs/access.log  /yuneta/bin/openresty/nginx/logs/error.log
```

`logrotate` rotates them each day, keeps 30, and compresses with
`delaycompress`. Thus the file of the previous day is `access.log.1`, and it is
not compressed. The normal daily path never needs gzip. There is no zlib in
`outputs_ext`, so a backfill older than two days is out of scope for phase 1
(§13).

### 3.1 Three generations of line format

The log format was extended twice. All three generations are in the 30 days of
rotated files, and two of them are in the same file until the next rotation.

| Generation | Fields | Quotes |
|---|---|---|
| combined | the stock nginx format | 6 |
| `+ $host` (2026-08-05) | `$host` at the end | 8 |
| `+ times` (this change) | `$request_time`, `$upstream_response_time` | 10 |

Each generation only appends. Everything before it stays the same, byte for
byte. Thus the field positions that `awk`, goaccess, and the factory fail2ban
filters use continue to work.

⚠️ **The parser must be tolerant, not strict.** A parser that demands 10 quotes
drops every line written before the change, and every line of a node whose
configuration is not updated yet. It must read what is there and leave the
rest unset.

⚠️ **Count the quotes on a copy.** In awk, `n=gsub(/"/,"x")` changes `$0`, and
the later `match` finds nothing. The same trap applies to any in-place scan.

## 4. The line sets the day window, not the file

This is the decision that keeps the yuno small.

The yuno does not keep a read offset, and it does not hook into `postrotate`.
To make the report of day D, it opens `access.log` and `access.log.1`. It reads
the `[$time_local]` of every line. It keeps the lines whose time is inside
`[D 00:00, D+1 00:00)`, in the time zone offset that the line itself carries.

What this gives:

- No state. No offset file, no inode tracking, nothing to repair.
- No coupling to logrotate. `logrotate.timer` fires when systemd wants.
  `delaycompress` moves the compression by one day. `sharedscripts` makes
  "which file is yesterday" ambiguous. None of it matters.
- Idempotent (safe to run again). You can run `report-day date=2026-08-05`
  again, and it gives the same record.
- Repairable. A yuno that was down does not lose the day, if the file is still
  there.

The cost is to read two files and not one. Measured on the five nodes, a day
is 1.5 MB to 3.5 MB. It is not a cost.

## 5. Architecture

```
webstats (yuno)
└── webstats  C_WEBSTATS       service, default_service
    ├── timer     C_TIMER      the daily schedule
    ├── whois     C_TIMER      the time of one lookup, and of its client's stop
    ├── reader    C_LOG_READER one per file being read (created, used, destroyed)
    └── whois     C_PROT_HTTP_CL one per lookup (created, used, destroyed)
        └── whois C_TCP        built by C_WEBSTATS, which listens to its EV_STOPPED

The two continuations of a run -- take the next file, read the next chunk --
are not timers. They are events the gobj posts to itself with
`gobj_post_event()`, delivered on the next cycle of the event loop.
```

### 5.1 C_WEBSTATS (service)

Owns the schedule, the accumulators, the report, and the commands.

States. The lifecycle is states, not flags. Thus a command that arrives at the
wrong moment fails loudly and names its sender.

| State | Meaning |
|---|---|
| `ST_IDLE` | waiting for the next scheduled run |
| `ST_READING` | one or more readers are feeding lines |
| `ST_LOOKING_UP` | asking the registries about the top clients (§7.3) |
| `ST_REPORTING` | building the report and handing it to `emailsender` |

Events in:

- `EV_TIMEOUT`: the schedule. In `ST_LOOKING_UP`, the lookup timer.
- `EV_LOG_LINES`, `EV_LOG_EOF`, `EV_LOG_ERROR`: from the readers.
- `EV_NEXT_FILE`: posted to itself.
- In `ST_LOOKING_UP`: `EV_ON_OPEN`, `EV_ON_MESSAGE`, `EV_ON_CLOSE` (from the
  lookup client), `EV_STOPPED` (from its C_TCP), and `EV_NEXT_LOOKUP` /
  `EV_LOOKUP_DONE` (posted to itself).

Events out: `EV_REPORT_READY` (`EVF_OUTPUT_EVENT|EVF_NO_WARN_SUBS`). With it, a
future consumer (controlcenter, a SPA view) can take the report and not touch
this yuno. It carries the report of the day, the one that is mailed. When a
run reads nothing for a day already stored with data, that is the stored
report, not the empty one that the run read.

Work starts in `mt_play`, not in `mt_create`. `mt_create` creates the timer.

Why `EV_NEXT_FILE` and not a deferred `EV_TIMEOUT`: a file ends inside the
publish stack of the reader, so the reader cannot be destroyed there. The
continuation must cross a cycle of the loop, and it carries its own name. The
`machine` trace says `EV_NEXT_FILE` and not `EV_TIMEOUT`. The FSM does not have
to use the state to tell the schedule and the continuation apart. The schedule
is still disarmed while a run goes, because `EV_TIMEOUT` means "it is the
hour", and only `ST_IDLE` knows what to do with it.

### 5.2 C_LOG_READER (child)

Turns one file into events. It knows nothing about nginx.

- Opens the file and reads it in bounded chunks, one chunk in each action, so
  the loop continues to run other work. A single read of a multi-megabyte file
  inside one action blocks the loop and hides the work from the `machine` trace.
- ⚠️ **io_uring is not used for this, and cannot be used yet.**
  `yev_create_read_event()` submits its reads with offset 0. That is correct
  for a socket and wrong for a regular file: every chunk reads the head of the
  file again. No gclass in the tree reads a regular file this way, so nothing
  showed it before. The reader uses `read(2)` and continues when it posts
  `EV_READ_CHUNK` to itself. The correct fix is to give yev a read offset. That
  is a kernel change, and it needs its own decision. The events that this
  gclass publishes do not depend on which of the two it uses.
- Splits chunks into lines and publishes `EV_LOG_LINES` with an array of
  complete lines.
- ⚠️ **A line crosses the chunk boundary.** The reader keeps the tail of a
  chunk that has no newline, and puts it before the next chunk. This is the
  classic bug of every chunked reader: it loses one line for each chunk. That
  is a small and stable percentage, so the numbers look plausible and are
  wrong.
- Ends with `EV_LOG_EOF` (with the line count) or `EV_LOG_ERROR`.
- CHILD subscription model: it publishes to its parent, which declares both
  events in its FSM.

### 5.3 Why the one timer left here is not polling

The framework forbids a timer that sends the same query again to see if
something changed. The daily timer in this yuno is a schedule. It fires one
time each day because the report is defined for each day, and no producer can
publish the event in its place. The continuations that looked like timers are
now posted messages, which is what they always were. The code carries this
comment, so that nobody removes the timer later as a discarded pattern.

The `whois` timer is not polling either. It is a limit: the maximum time of
one lookup. C_TCP tries a failed connection again and tells nobody while it
does it. Without this limit, a registry that is down holds the daily run in
`ST_LOOKING_UP` for ever.

## 6. Configuration

Attributes of `C_WEBSTATS`. You can set all of them from the batch configuration.

| Attribute | Type | Default | Meaning |
|---|---|---|---|
| `access_log_paths` | list | both trees | files to read for the access log |
| `error_log_paths` | list | both trees | files to read for the error log |
| `report_hour` | int | 6 | local hour of the daily run |
| `report_minute` | int | 0 | minute of the daily run |
| `send_email` | bool | true | false leaves the record and skips the mail |
| `email_to` | str | — | destination |
| `email_from` | str | — | sender. `"(^^__hostname__^^)@domain"` names the node. Empty leaves it to the email service |
| `email_service` | str | `emailsender` | service that sends |
| `top_n` | int | 20 | rows in each top table |
| `max_distinct_keys` | int | 200000 | cap for each counter map (§8.3) |
| `rotation_stall_minutes` | int | 120 | a `<path>.1` newer than its `<path>` by more than this says the server never reopened (§9.1.1). `0` disables the check |
| `probe_patterns` | list | mirrors the fail2ban filter | what counts as a probe |
| `internal_networks` | list | — | prefixes not counted as clients |
| `keep_days` | int | 400 | days of aggregates kept |
| `asset_extensions` | list | `.js`, `.css` | what a browser fetches to draw a page |
| `bot_agents` | list | the usual marks | a user agent that says it is a crawler |
| `new_visitor_days` | int | 30 | days of history that decide whether a visitor is new |
| `visitor_salt` | str | — | salt of the visitor fingerprint (§8.5) |
| `fail2ban_log_path` | str | `/var/log/fail2ban.log` | the log of fail2ban (§7.4). The yuno also reads its last rotation. Empty: not read |
| `whois_enabled` | bool | true | look up the country and organization of the top clients (§7.3) |
| `rdap_url` | str | `https://rdap.db.ripe.net/ip/` | RDAP service. The yuno appends the address. It must be `https` |
| `whois_rows` | int | 10 | rows of each table of clients that are looked up |
| `whois_cache_days` | int | 30 | an answer younger than this is taken from the stored days |
| `whois_timeout` | int | 15000 | maximum milliseconds of one lookup (each redirect hop has its own) |

`report_hour` defaults to 06:00 and not to 00:05 on purpose. The window is
taken from the timestamps, so there is no reason to race `logrotate`. Also, a
report that is built at six is in the mailbox when somebody opens it.

Every count of days uses the calendar (`local_day.c`): the oldest day that
`keep_days` keeps, the `new_visitor_days` of history, the days back of the
comparison, and the age of a `whois_cache_days` answer (the same time of day,
N days before). Up to 7.25.20 they were `now - N*86400`, one hour off across
each change of hour. Near midnight, the oldest day kept, or the window of new
visitors, moved by one day two times each year.

The stat `next_run` tells the time of the armed schedule (epoch seconds). A run
reports the day before its slot (the scheduled time of a run). When the run
ends, the next armed slot is always one whose DAY has not run
(`next_slot_after()` in `local_day.c`). Two cases needed this:

- A timer that fires a little before its slot, with a run that also ends
  before the slot, found that same slot still ahead.
- On the day of the autumn change, a `report_hour` of 2 resolves to the SECOND
  02:00, which reports the same day as the first.

Up to 7.25.20, both cases armed the slot again: the day ran, and was mailed,
two times. Each slot is built from its date and `report_hour:report_minute`.
On the day after the spring change, a 02:30 that fell in the gap is 02:30
again, not the 03:30 that it was normalized to.

`internal_networks` is important for the numbers. On `artgins`, the clients
that hammered Keycloak were our own nodes. Without this list, the "top clients"
table shows the suite talking to itself.

## 7. What it computes

### 7.1 Access log

- Visitors, and how many are new. This is the number that the report exists
  to give: how many people used the site, not how much traffic arrived.
  A visitor is an address that asked for a piece of the page and got it (2xx).
  A piece of the page has an extension of `asset_extensions`, `.js` or `.css`
  by default. Also, the user agent of a visitor carries no mark of `bot_agents`. That is the honest signal. A
  browser fetches the page and its sub-resources. A scanner with a browser
  user agent asks for one URL and leaves. Measured on 2026-08-06 on a staging
  node: 1346 addresses claimed to be a browser, and 71 ever fetched a script.
  The yuno excludes a declared crawler, although Googlebot does run JavaScript.
  `full_page` counts the visitors that fetched five pieces or more, which is
  what it costs to draw a whole page.
  A new visitor is a visitor whose fingerprint is in none of the last
  `new_visitor_days` (30) days that are stored. The record tells over how many
  days the yuno compared it. A day that is not in the store cannot tell that
  somebody was there, so on a fresh install everybody is new.
  ⚠️ **It is a floor, not a census.** Several people behind one NAT count as
  one. A returning reader with a warm cache fetches no asset at all. A site
  with no JavaScript or CSS has no visitors by this measure. It answers "did
  anybody use this today", and it does not answer "how many people".
- Totals: requests, bytes, distinct clients, status classes 2xx/3xx/4xx/5xx.
- For each hour: 24 counters.
- For each vhost (`$host`): requests, the 2xx / 4xx / 5xx split, bytes, and its
  own latency histogram. `$host` was added for this.
  The 2xx column answers the question that the other two cannot: how much of
  the traffic served somebody. A vhost with 205 requests, all 205 of them 4xx,
  is not a site with visitors. It is a name that somebody scans.
  Byte counts print in KB/MB/GB. On `145695002`, the reader has to count the
  digits.
  The mail shows the busiest `top_n` and tells how many more answered. The name
  is the Host header, so anybody can invent one. The first mail from a staging
  node listed 52 rows, and half of them were forged names with one request each.
  The yuno counts distinct clients one time, globally, and not for each vhost.
  A set of addresses for each name multiplies the worst case by the number of
  vhosts, and the number that people read is the total.
  A line of the first generation has no `$host`, so it goes under `-`. That
  name is the mark of a node whose configuration is not updated yet.
- Every 5xx in full, with time, vhost, request, status, and client. Not a
  counter, but the lines. Capped, and the record reports the cap (§8.3).
- Top paths, top 404 paths, top referrers, top user agents, top clients.
  The 404 table separates real broken links from probes, which the yuno counts
  separately.
- Probes: requests that match `probe_patterns` (any `.php`, `.env`, `.git`,
  `.aws`, `.ssh`, `.svn`, `.hg`, `/wp-*`, `/cgi-bin`), with the count, the
  distinct clients and the top offenders.
  ⚠️ **The filter must not key on the status code.** A SPA vhost
  (`try_files … /index.html`) answers 200 to any path. That is the bug that
  made the packaged fail2ban filter blind on both console vhosts.
  The yuno matches the patterns against the percent-decoded path. Thus a
  scanner that writes `/%2eenv` is counted with the scanners that write
  `/.env`. Write the patterns only in their plain form. To add the encodings to
  the list is a game with no last move (`%2E`, `%2f`, and the combinations of
  both).
- Latency, when the log format carries it (§10): count, sum, max, and a
  fixed histogram, for each vhost and in total.
  Buckets, in seconds:
  `0.005 0.01 0.025 0.05 0.1 0.25 0.5 1 2.5 5 10 +inf`.
  A histogram and not a list of samples: the memory is bounded for any
  traffic. The buckets of two days can be added, so the weekly view costs
  nothing.
  p50/p95/p99 are the upper edge of the bucket that the percentile falls in,
  not an interpolation. "p95 is at most 0.5 s" is true. A number invented
  between two edges is not true. A percentile above the last edge is `-1` in
  the record and prints as `>10s` in the mail. Printed as a number, it looks
  like a broken field, not like an answer.
  The mail also tells over how many requests the yuno measured the latency.
  Only the newest `log_format` carries a time. Thus on the day that the format
  changed, it covered 2034 of 8711 requests. A reader who does not know that
  reads the p95 as the whole day.
  ⚠️ **A percentile is not a diagnosis.** On 2026-08-06 `yuneta.io` showed a
  p95 of 10 s. All 18 slow requests were one client in a two-minute window.
  It fetched static `/build/*.js` over HTTP/2 with no upstream, and most of
  them delivered 0 bytes. Their times came in identical clusters
  (26.789 ×5, 28.312 ×9): one connection whose multiplexed streams all ended
  together. `$request_time` counts the read speed of the client, so a stalled
  browser looks exactly like a slow server. Read `ups=` and the client before
  you believe a tail.
  ⚠️ **The bucket index is a ceiling division.** A truncation puts the p95 of
  two measures on the first bucket. Then a day where one of two requests took
  half a second reports as a fast day.

### 7.2 Error log

The real findings were in this half. On 2026-08-05 the error logs held the
Keycloak connection refusals, the `client_max_body_size` hits of real users,
and 185 `bind() Address already in use` on `node-b`. These came from an
automation that believes that it starts nginx, and fails silently. None of it
was in the access log.

The yuno groups the lines by signature: the message without its variable parts
(`pid#tid`, `*connection`, `client:`, `server:`, `request:`, `upstream:`,
`host:`, and any number). For each signature, the record keeps the count, the
first and last time, and one full sample line.

⚠️ **Count it as one event, not two.** An `error_page 404 /404.html` that points
at a file that does not exist writes one error line, the one for the failing
`404.html`. That line names the original request.

### 7.3 Who the top clients are

An address tells nothing to the reader of the mail. `45.148.10.1` with 600
probes is a number. `AD · TECHOFF SRV LIMITED (DMZHOST)` is somebody. Thus the
rows of Top clients and Top offenders carry the country, the organization that
holds the network, the network name, and its range.

The source is RDAP, the JSON successor of whois, over HTTPS:

```
GET https://rdap.db.ripe.net/ip/198.51.100.23
Accept: application/rdap+json

200  {"name": "EXAMPLE-NET-1", "country": "FR",
      "startAddress": "198.51.100.0", "endAddress": "198.51.100.255",
      "entities": [{"roles": ["registrant"], "handle": "ORG-EX1-RIPE",
                    "vcardArray": ["vcard", [["fn", {}, "text", "Example Hosting"], …]]}, …]}
```

One service answers for every address. RIPE sends an address that is not in
its registry to the registry that holds it, with a `301` (`8.8.8.8` → ARIN,
`1.1.1.1` → APNIC). The lookup follows up to three hops. It follows only
`https`.

The alternatives, and why the yuno does not use them:

- Not port 43 whois: each registry writes its own text format, and that is
  five parsers to keep.
- Not a GeoIP database: it needs a license and a monthly update on every node.
- Not a third-party API: it sends the addresses of our clients to somebody
  else.

What the yuno reads from the answer:

| Field | From |
|---|---|
| `country` | the `country` of the network. ARIN and LACNIC leave it null. Then it is the address of the registrant: its country field, or the last line of its label (ARIN writes a name there, `United States`, not a code) |
| `org` | the `fn` of the registrant. RIPE and APNIC list the organization (`ORG-…`) and its maintainers (`…-MNT`) as registrant. The organization is the one with the name |
| `net` | the `name` of the network |
| `range` | `startAddress - endAddress` |
| `source` | the registry that answered |
| `looked_up_at` | when |

The yuno never asks about a private, loopback, or link-local address, because
it belongs to no registry. The same test lets the key of the log into the URL
only when the key is an address. The person who made the request wrote that
key.

The stored days are the cache. Before it asks, the run reads the records of
the last `whois_cache_days` days, and takes any answer younger than that. For
a day reported again, these days include the reported day. The records
already hold the top clients of every day, with what the yuno learned about
them. Thus a second store only repeats them. In steady state, a run asks for
the few addresses that it did not see this month. This also keeps the rate
limits of the registries out of the picture.

A lookup never stops the mail. The yuno asks about one address at a time,
each with a client built for it and destroyed after it. A registry can fail
to answer, answer something that is not RDAP, or refuse. Then the yuno writes
`{"error": "…",
"looked_up_at": …}` on that row, a WARNING in the log, and `lookup failed: …`
in the mail. The yuno never takes a failure from the cache: the next run asks
again.

The record tells how the yuno got the names: `"whois": {"enabled": true,
"cached": 7, "looked_up": 3, "failed": 0}`, printed under Sources.

⚠️ **Everything in an RDAP answer is escaped before it goes in the mail**, the
same as the log. The holder of the network writes the registry records.

⚠️ **The body arrives as a gbuffer, not as json.** RDAP answers
`application/rdap+json`, and the http parser turns only `application/json`
into json. The gbuffer belongs to the kw, so the parse takes its own reference.

### 7.4 Who fail2ban banned

On each row of *Top clients* and *Top offenders*, the report tells whether
fail2ban banned that address on the day. Without it, the mail counts probes
and cannot tell whether anybody did anything about them. Also, a jail that
bans nobody looks exactly as healthy as a jail that works:
`fail2ban-client status` answers the same for both. That is how the probe jail
sat blind on two SPA vhosts.

The yuno reads `fail2ban_log_path` and its last rotation, with the same rule as
the web logs (§4): the day of a line is its own timestamp. Debian names the
rotation `fail2ban.log.1`. RHEL rotates with a date (`fail2ban.log-20260906`),
and then the yuno reads the newest uncompressed one.

Only these lines count:

```
2026-09-21 05:46:26,740 fail2ban.actions  [837]: NOTICE  [yuneta-nginx-probe] Ban 35.205.254.119
2026-09-21 05:46:26,801 fail2ban.observer [837]: NOTICE  [yuneta-nginx-probe] Increase Ban 35.205.254.119 (3 # 4d 00:00:00 -> 2026-09-25 05:46:26)
```

`Ban` is a ban. `bantime.increment`
(`tools/fail2ban/install-probe-ban-escalation.sh`) writes `Increase Ban`. It
tells which ban of that address this is, and how long it lasts. That number
shows a scanner that returns. `Restore Ban` (a ban put back when fail2ban
restarts) and `Unban` are not new bans, and the yuno does not count them.

A row gets `"banned": {"bans": 1, "at": "05:46", "jails": ["yuneta-nginx-probe"],
"ban_number": 3, "ban_time": "4d 00:00:00"}`, or `"banned": false`. The mail
prints `banned 05:46 #3 (4d 00:00:00)` or `not banned`.

⚠️ **`"banned": false` is written only when the log WAS read.** When the yuno
fails to read the log, the rows carry nothing. The record tells why
(`"fail2ban": {"read": false, "error": "cannot open file"}`), with a line in
Needs attention. "Not banned" and "did not look" must never print the same.
RHEL ships the log `root:root 0600`.
`tools/fail2ban/make-fail2ban-log-readable.sh` makes it `root:adm 0640`, as
Debian does.

Needs attention also carries a line when no top offender that probed three
times or more (the `maxretry` of the jail) was banned. Then the jail
probably watches nothing.

⚠️ **A ban lands at the end of a scan, not during it.** A scan is 200-300
requests in two to five seconds, and fail2ban reads the log after nginx wrote
it. Thus the scanner shows in *Top offenders* with its whole count AND
banned. The ban gives time until the scanner returns. The escalation is for
that.

## 8. Persistence

TimeRanger2, one topic.

- Topic `daily_stats`, pkey `date` (`YYYY-MM-DD`), `sf_string_key`, one record
  for each day, a few kilobytes each. `keep_days` bounds it. At every play and
  at the end of every run, the yuno drops the keys that sort before
  `today - keep_days`. An ISO date sorts the same as it reads. That is why the
  key has that shape.

The yuno never stores raw lines.

A day reported two times has two records under the same key, and the newest
one is the answer. Thus you can repeat `report-day` with no delete first: a day
rebuilt after a fix wins.

⚠️ **A run that reads nothing does not replace a run that read something.**
To rebuild a day whose log has already rotated away reads zero lines. To store
that result overwrites a good record with an empty one, silently, because the
newest record is the one that answers. Found when 2026-08-05 was rebuilt on
the 7th: the day was gone from disk, and the report came back empty. Every
visitor of the next day looked new, because the day before held nobody. The
yuno abandons such a run, with a warning in the log, and sends no mail.

⚠️ **To read the newest record, do not use `(from_rowid=1, limit=1, backward=TRUE)`.**
`from_rowid` is a position among the rows that the iterator returns.
`backward` does not change it into a position from the end. Thus that call
gives row 1, the oldest. Ask `tranger2_iterator_size()` for the row count, and
read that rowid forward. With the wrong call, `get-report` answered for ever
with the first version of a day. A run to correct a day changed nothing that
anybody can read. Found when one day was rebuilt two times and the store was
examined.

### 8.5 Visitor fingerprints

The record keeps a fingerprint for each visitor, never the address. The store
lives 400 days. This yuno must not hold a 400-day list of the addresses of the
people who read the site. A fingerprint compares and counts exactly as well.

⚠️ **It is not a security measure.** IPv4 is small enough to walk the whole
space against a plain hash. Where that is important, set `visitor_salt`. Set it
one time and do not change it. A new salt makes all visitors of every earlier
day look like strangers.

### 8.4 What changed

The record carries a `changed` block: the headline numbers of the previous
day, the median of the last seven days, and the numbers of today. That is the
whole reason to keep a history. A number with nothing to compare it to carries
no information.

A day with no history says `days_of_history: 0`, and does not compare with
zero. Compared with zero, the first morning reads as *everything doubled*.

### 8.1 Record shape (version 3)

```json
{
    "date": "2026-08-05",
    "version": 1,
    "node": "node1",
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
    "top": {"paths": [], "not_found": [], "referrers": [], "agents": [],
            "clients": [{"key": "198.51.100.23", "count": 9,
                         "whois": {"country": "FR", "org": "Example Hosting",
                                   "net": "EXAMPLE-NET-1",
                                   "range": "198.51.100.0 - 198.51.100.255",
                                   "source": "rdap.db.ripe.net",
                                   "looked_up_at": 1790072400}}]},
    "server_errors": [{"host": "", "client": "", "status": 500, "path": "", "hour": 0}],
    "probes": {"requests": 0, "clients": 0, "top_patterns": [], "top_clients": []},
    "whois": {"enabled": true, "cached": 0, "looked_up": 0, "failed": 0},
    "fail2ban": {"enabled": true, "read": true, "bans": 0, "banned_clients": 0,
                 "by_jail": {"yuneta-nginx-probe": 0}},
    "errors": {"total": 0, "distinct": 0, "by_signature": [
        {"signature": "", "count": 0, "first": "", "last": "", "sample": ""}
    ]},
    "truncated": [{"counter": "paths", "dropped": 0}]
}
```

Every `top` row is `{"key": …, "count": …}`. The first `whois_rows` rows of
`top.clients` and `probes.top_clients` also carry `whois` (§7.3), unless the
address is private. Every row of those two tables carries `banned` when the
yuno read the log of fail2ban (§7.4). Version 2 added `whois`. Version 3 added
`banned` and `fail2ban`. An older record is the same without them. `latency`
keeps the raw buckets, so that two days can be added. `latency_summary` is what
a reader looks at. `sources[].lines` is what the file holds, `kept` is what
fell inside the day, and `unparsed` is what the parser did not understand. A
file that the yuno did not read shows `error` in place of the counts, for any
cause: the file is absent, or the reader for it was not built. Every file of
the run is in `sources`, in one form or the other. A report that drops a
source and does not say so reads as a report of everything.

`version` is in the record because the shape will grow. A reader that finds a
version that it does not know says so. It does not guess.

### 8.2 Nothing is silently dropped

- `sources[].unparsed` counts, for each file, the lines that the parser did
  not understand. A parser that quietly skips what it cannot read reports a
  quiet day.
- `truncated` lists every cap that the yuno hit, with the counter and the
  number of keys dropped. A cap that is not reported reads as full coverage.

### 8.3 Caps

Counter maps are capped at `max_distinct_keys`. A day under attack can produce
a very large number of distinct paths. With an unbounded map, the report
generator becomes the incident. When a map reaches the cap, it stops accepting
new keys and continues to count the ones that it has. It also adds an entry to
`truncated`.

## 9. The report

The report is a mail that a busy person must read every day. Thus it is short,
and it is ordered by what needs a decision.

1. Needs attention: 5xx by vhost, new error signatures that were not in the
   previous days, latency above the previous week, a source file that the yuno
   cannot read, zero lines parsed.
2. What changed: requests, bytes, 4xx, 5xx, probes, p95. Each one shows the
   previous day and the median of the last 7 days, read from `daily_stats`. A
   number with no comparison carries no information.
3. Traffic: for each vhost, for each hour, and the top tables.
4. Errors: signatures with counts and one sample.
5. Probes: count, clients, top offenders.

The body is HTML, self-contained: no image and no external stylesheet. The
styles are inline, because a mail client throws a `<style>` block away.

⚠️ **The body must be broken into lines.** SMTP allows at most 1000 octets on
each line (RFC 5321), and this report is one single line of 40 KB. A relay has
the right to fold it, and OVH does. It inserts CRLF+space every ~1000 bytes, in
the middle of whatever is there. The first mail from a staging node arrived
with `&Delta;` split into `& Delta;`, a row label that read `cl ients`, a path
that read `ac cess.log`, and a `<td` cut in half. That `<td` stops being a tag
and prints as text in the middle of a table. A newline goes between every pair
of adjacent tags. It can never split a word, because the text of the report
holds no raw `>` after escaping, and HTML collapses the whitespace. The yuno
draws the hour histogram with `div` widths, so the mail asks no server for
anything when it is opened. `emailsender` builds the MIME part from `body`
with `is_html`.

⚠️ **Every IPv4 address of the text is written `[a.b.c.d]`.** The relay of OVH
read `34.140.132.132` as a Spanish phone number and delivered the mail to
nobody (7.25.19). An address attached to a word, a slash, or a dot stays as it
is, because it is a version. A character that only separates stays outside the
brackets:

| In the text | In the mail |
|---|---|
| `client 34.140.132.132.` | `client [34.140.132.132].` |
| `upstream 34.1.2.3:443`, `34.1.2.3: refused`, `client:34.1.2.3` | `upstream [34.1.2.3]:443`, `[34.1.2.3]: refused`, `client:[34.1.2.3]` |
| `range 10.0.0.1-10.0.0.9` | `range [10.0.0.1]-[10.0.0.9]` |
| `::ffff:34.1.2.3`, `64:ff9b::34.1.2.3` | `[::ffff:34.1.2.3]`, `[64:ff9b::34.1.2.3]` (the whole IPv6 address) |
| `Chrome/142.0.0.0`, `nginx-1.25.3.1`, `1.2.3.4-beta`, `12:30:45.123` | unchanged |

Up to 7.25.20 a colon or a dash on either side left the address bare.

⚠️ **Every string that came out of the log is HTML-escaped.** This is not
decoration. The person who made the request writes the path, the referer, and
the user agent. Without escaping, a probe for `/<img src=x onerror=…>` arrives
as live markup inside our own mailbox: the scanned node runs a script on the
reader of the report about the scan. Tested with exactly that request and a
`<script>` user agent through a real run: both come out as `&lt;img` and
`&lt;script`.

The subject carries the verdict of the day, never the same text two times in a
row: `NO DATA`, or `N 5xx, M requests`, or `M requests, N errors`. A subject
that reads the same every morning trains the reader to leave it unopened. That
is the end of a daily report.

⚠️ **No attachment by default.** `emailsender` reads `attachment` as a path in
its own process. If the `emailsender` of the node is reached through the agent
on another node, the path does not exist there. Then the attachment is lost or
wrong. The full JSON stays in TimeRanger2, and you get it with `get-report`.

### 9.1 Silence is a failure

If the yuno parses zero lines for the day, it still sends the mail, and the
subject says so. A report generator that says nothing when it is broken looks
the same as a quiet day. That is exactly how a fail2ban jail sat healthy for
months and banned nobody. The rule from that session applies here: a control
that declares itself healthy is not tested.

For the same reason, `list-sources` exists (§11). It tells which files the yuno
will read, whether each one exists, and whether the yuno can read it. Run it
after the install, and do not assume.

### 9.1.1 A rotation that stopped rotating

Readable is not the same as healthy. A log rotation is two steps: logrotate
renames the file, and the web server reopens it. When the second step is lost,
the server continues to write to the old descriptor. `<path>.1` grows while
`<path>` stays as logrotate created it. Nothing reports it. The server serves,
logrotate exits 0, and this yuno still builds the report, because it reads both
files.

It also does not repair itself. When `<path>` is empty, `notifempty` skips it,
so logrotate never tries the rotation again. One lost signal costs the rest of
the life of the node. It cost eight days on a Rocky 9 test node (23–31 August
2026). Somebody found it when they listed the directory.

The signature needs no history. After a healthy rotation, the server writes the
live file, so its mtime runs ahead of the `.1`. The other way round means that
the reopen was lost. `rotation_stall_minutes` (default 120, `0` disables the
test) is the margin for a site with no traffic since the rotation. On such a
site, neither file moves, and neither is meaningfully newer. The margin keeps
that site from reading as broken.

The yuno reports it in three places, because the three have different readers:

- A `WARNING` in the log, at the moment that the yuno sees it. Operators watch
  the node from there, and the mail is one time each day.
- `stalled_rotations` in the stored record, and a line in Needs attention of
  the mail.
- `list-sources`, which answers error and names both files and the gap. Thus
  the command that an operator runs to test the yuno also tests the rotation.

The yuno does NOT fix it. To send the reopen signal to a web server is not the
work of this yuno. A report generator that restarts services is a different and
worse thing.

### 9.2 Delivery

`required_services: ['emailsender']` in the yuno configuration, then:

```c
hgobj gobj_emailsender = gobj_find_service("emailsender", FALSE);
gobj_send_event(gobj_emailsender, EV_SEND_EMAIL, kw_email, gobj);
```

`kw_email` carries `to`, `subject`, `body`, `is_html`. `logcenter` already uses
this path. The body goes as a string, not as a `gbuffer`. `emailsender`
changes a `gbuffer` into `body` in any case, and a string does not touch the kw
auto-decref trap.

If the yuno cannot reach `emailsender`, it logs the failure and keeps the
report in `daily_stats`. Thus you can send the mail again with
`report-day date=… send=1`.

## 10. The nginx side

`$request_time` and `$upstream_response_time` are added at the end, with the
same discipline as `$host`: everything before them stays the same, byte for
byte.

```nginx
log_format vhost '$remote_addr - $remote_user [$time_local] "$request" '
                 '$status $body_bytes_sent "$http_referer" "$http_user_agent" '
                 '"$host" $request_time "$upstream_response_time"';
```

- `$upstream_response_time` is quoted. It is `-` with no upstream, and a list
  (`0.001, 0.002`, or `0.001 : 0.002` with an internal redirect) with more than
  one upstream. Without quotes, it breaks the field count.
- `$request_time` is a plain number and needs no quotes.
- The quote count goes 6 → 8 → 10. Thus you can read the generation of a line
  at a glance, and so can the parser.

Rollout. The configurations are not in this repo. An operations repo owns each
one, and its own `deploy-conf.sh` uploads it. Thus the next upload overwrites a
configuration edited in place on a node. `artgins` and the `yuneta.io` root are
in the artgins ops repo on branch `master`. The domains of each application are
in the repo of that application, one for each node. To roll out:

1. Patch the repo and the node in the same session, and `diff` them.
2. Run `nginx -t` before the reload.
3. On `artgins`, take a `curl` picture of the other domains before and after.
   A reload there also touches Keycloak, code, doc, and www.

Until a node is updated, its lines have 8 quotes, and the latency block of the
report of that node is empty. That is expected. The report says it, and does
not show zeros.

## 11. Commands

| Command | What it does |
|---|---|
| `analyze-now` | build the report of yesterday, now |
| `report-day report_date=YYYY-MM-DD [send=1]` | build again any day that is still on disk. Sends only with `send=1` |
| `get-report report_date=YYYY-MM-DD` | the stored aggregate record, newest version of that day |
| `list-reports` | the dates held in `daily_stats` |
| `list-sources` | the files that it will read, whether each is readable now, and whether any rotation is stalled (answers error if one is) |
| `preview-report report_date=YYYY-MM-DD` | the mail body of a stored day, as HTML, without sending it |

⚠️ **The day parameter is `report_date`, not `date`.** `command-yuno` gives its
WHOLE kw to the node query that selects the yuno. Thus a parameter with the
name of a field of the yuno record becomes a filter on that field. The yuno
record has a `date` (when it was created). Thus `command-yuno id=91 command=report-day
date=2026-08-05` matches no yuno and answers "Yuno not found". It names the
yuno, never the parameter. This is the documented `id=` collision, and it is
not limited to `id`. Before you name a parameter, read the columns of
`list-yunos`. The handlers still read a plain `date` as a fallback, for a
caller that reaches the gclass directly.

## 12. Trace levels

| Level | Shows |
|---|---|
| `read` | file opened, chunks, lines, EOF |
| `parse` | lines the parser rejected, with the line |
| `report` | the built report before it is handed to `emailsender` |
| `whois` | each RDAP request (url) and the status of each answer |

`parse` is the important level. It is the only way to see a format change that
the parser silently tolerates.

## 13. Phases

Done: the pipeline and every aggregate of section 7. That is the tolerant
parser of the three generations, totals, the counters for each hour and each
vhost, the tops, all 5xx whole, the probes, the latency histogram with its
percentiles, and the error signatures. The record reports the caps and the
unparsed lines.

Tested against a log of ten lines, built to hit each case:

- The three generations of format.
- A query string that the parser must cut off the path.
- A probe answered with 200 on a SPA vhost.
- A client of `internal_networks`.
- A malformed line, and lines of two other days.
- Two `connect() failed` lines with different pid, connection, and client,
  which must fold into one signature.

It counted 8 requests of 9 kept from 10 lines, 5 distinct clients of 6, the
2xx/4xx/5xx split, 2 probes from 2 clients, and 2 signatures from 3 error
lines. No error, no warning, no leak on shutdown.

Store, done: the `daily_stats` topic, the pruning by `keep_days`, the
`changed` block against the previous day and the median of the week, and
`get-report` / `list-reports` on top of it.

Tested under a real agent on the dev node, against the 126861 lines of its own
access.log. Three days were built with `report-day`, listed by `list-reports`,
and read back by `get-report`. One day was built a second time, to prove that
the newest version wins and that the rebuild finds its two days of history.
Orderly shutdown with no error and no leak.

Report, done: the HTML body of section 9, ordered by what needs a decision,
and the subject that gives the verdict of the day. `preview-report` shows the
body of a stored day and sends nothing. Thus you can look at a change to the
report before it reaches a mailbox.

`report-day` refuses a day older than `keep_days`. It does not read the logs
and write a record that the pruning of that same run then drops. To do the work
and throw it away is worse than to say no. The command answered
*"reading the logs"*, the mail arrived, and then `get-report` said that the day
did not exist. Found when somebody asked for a day from January.

Phase 1 is complete. Phase 2 is what is left (§13 below).

Not done, by design: the mail carries no attachment (§9). The store keeps only
the top of each counter, so *"new error signature"* means *not among the
top signatures of yesterday*.

Phase 2:

- gzip, to rebuild any of the 30 days on disk. Needs zlib in `outputs_ext`.
- One mail for the five nodes, assembled in controlcenter, in place of five.
  With five daily mails, soon nobody reads any of them.
- An immediate alarm on a 5xx burst, so that nobody waits for the daily mail.
- A view in the agent SPA over `get-report`.

## 14. Open questions

- The retention of `keep_days=400` is for year-over-year comparison. It is a
  guess until we see the record size on a busy node.
- The probe pattern list is in two places: here, and in the fail2ban filter
  that the packagers write. The two will drift apart. A single source in
  `tools/` is a good later change.
