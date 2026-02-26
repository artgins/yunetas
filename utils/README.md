# utils

Command-line utilities for operating, inspecting, and developing with Yuneta. All tools install to `${BIN_DEST_DIR}` (typically `outputs/bin/`, deployed to `/yuneta/bin/`).

## Directory Layout

```
utils/
├── c/
│   ├── ycommand/           # Send commands to running yunos (interactive or one-shot)
│   ├── ybatch/             # Execute batch command files against remote yunos
│   ├── ytests/             # Run test suites against remote yunos
│   ├── ystats/             # Monitor statistics and attributes of running yunos
│   ├── ylist/              # List running yuno processes
│   ├── yshutdown/          # Shut down yuno processes and the agent
│   ├── tr2list/            # List records in TimeRanger2 databases
│   ├── tr2search/          # Search within record content in TimeRanger2 databases
│   ├── tr2keys/            # List keys/primary keys in TimeRanger2 topics
│   ├── tr2migrate/         # Migrate data from TimeRanger v1 to TimeRanger2
│   ├── treedb_list/        # List records in TreeDB hierarchical databases
│   ├── list_queue_msgs2/   # List messages in TimeRanger2 queues
│   ├── fs_watcher/         # Monitor filesystem changes (io_uring-based)
│   ├── inotify/            # Test inotify filesystem event monitoring
│   ├── yclone-gclass/      # Clone and rename a GClass
│   ├── yclone-project/     # Clone a project with keyword substitution
│   ├── ymake-skeleton/     # Create skeleton templates from existing projects
│   ├── yuno-skeleton/      # Generate new yunos and GClasses from templates
│   ├── yscapec/            # Escape a file into a C string literal
│   ├── ytestconfig/        # Validate JSON configuration files
│   ├── pkey_to_jwks/       # Convert RSA public keys to JWKS format
│   └── test-musl/          # Minimal test for musl static builds
└── python/                 # (reserved, currently empty)
```

## Tools by Category

### Control Plane

Tools that communicate with running yunos over WebSocket. They share a common set of connection and OAuth2 options.

#### ycommand

Send commands to running yunos interactively or as one-shot operations.

```
ycommand [OPTIONS]

Remote Service:
  -c, --command COMMAND      Command to execute
  -i, --interactive          Interactive mode (ncurses UI)
  -w, --wait SECONDS         Wait time before exit (default: 2)

OAuth2:
  -K, --auth_system SYSTEM   OpenID system (default: keycloak)
  -k, --auth_url URL         OpenID endpoint
  -Z, --azp AZP              Authorized Party (client_id)
  -x, --user_id USER_ID      OAuth2 user ID
  -X, --user_passw PASSWORD  OAuth2 user password
  -j, --jwt JWT              Pre-obtained JWT token

Connection:
  -u, --url URL              URL to connect (default: ws://127.0.0.1:1991)
  -O, --yuno_role ROLE       Remote yuno role (default: yuneta_agent)
  -o, --yuno_name NAME       Remote yuno name
  -S, --yuno_service SERVICE Remote service (default: __default_service__)

Local:
  -p, --print                Print configuration
  -r, --print-role           Print yuno information
  -l, --verbose LEVEL        Verbose level
  -m, --with-metadata        Print with metadata
  -v, --version              Print version
  -V, --yuneta-version       Print Yuneta version
```

Uses ncurses/panel for the interactive terminal UI. Links `CONSOLE_LIBS`.

Examples:

```bash
ycommand -c 'help'                              # list available commands
ycommand -c 'stats' -u ws://10.0.0.1:1991       # get stats from remote yuno
ycommand -i                                      # interactive mode
```

#### ybatch

Execute batch command files against remote yunos.

```
ybatch [OPTIONS] [FILE]

OAuth2:
  -K, --auth_system SYSTEM   OpenID system (default: keycloak)
  -k, --auth_url URL         OpenID endpoint
  -Z, --azp AZP              Authorized Party (client_id)
  -x, --user_id USER_ID      OAuth2 user ID
  -X, --user_passw PASSWORD  OAuth2 user password
  -j, --jwt JWT              Pre-obtained JWT token

Connection:
  -u, --url URL              URL to connect (default: ws://127.0.0.1:1991)
  -O, --yuno_role ROLE       Remote yuno role
  -o, --yuno_name NAME       Remote yuno name
  -S, --yuno_service SERVICE Remote service (default: __default_service__)

Local:
  -p, --print                Print configuration
  -f, --config-file FILE     Load settings from JSON config file
  -r, --print-role           Print yuno information
  -l, --verbose LEVEL        Verbose level
  -v, --version              Print version
  -V, --yuneta-version       Print Yuneta version
```

#### ytests

Run test suites against remote yunos.

```
ytests [OPTIONS] [FILE]

  -t, --repeat TIMES         Repeat execution count (default: 1, -1 for infinite)

  (plus the same OAuth2, Connection, and Local options as ybatch)
```

#### ystats

Monitor statistics, attributes, and commands of running yunos.

```
ystats [OPTIONS]

Remote Service:
  -t, --refresh_time SECONDS Refresh interval (default: 1)
  -s, --stats STATS          Statistic to query
  -a, --attribute ATTRIBUTE  Attribute to query
  -c, --command COMMAND      Command to execute
  -g, --gobj_name GOBJNAME   Target GObj (named-gobj or full-path)

  (plus the same OAuth2, Connection, and Local options as ycommand,
   including -m/--with-metadata)
```

### Process Management

#### ylist

List running yuno processes. Reads PID files from the runtime directory, verifies processes are alive, and cleans up stale PID files.

```
ylist [OPTIONS]

  -p, --pids                 Display only PIDs (one-liner format)
```

#### yshutdown

Shut down yuno processes and/or the Yuneta agent.

```
yshutdown [OPTIONS]

  -l, --verbose              Verbose mode
  -n, --no-kill-agent        Don't kill the Yuneta agent
  -s, --no-kill-system       Don't kill system yunos (logcenter)
  -a, --kill-only-agent      Kill only the agent
```

### TimeRanger2 Database Tools

Tools for inspecting and managing TimeRanger2 append-only time-series databases.

#### tr2list

List records in TimeRanger2 databases with filtering and formatting options.

```
tr2list [OPTIONS] PATH

Database:
  -r, --recursive            List recursively through subdirectories
  -t, --print-local-time     Display times in local timezone

Presentation:
  -l, --verbose LEVEL        empty=total, 0=metadata, 1=metadata,
                              2=metadata+path, 3=metadata+record
  -m, --mode MODE            Display mode: form or table
  -f, --fields FIELDS        Show only specified fields
  -d, --show_md2             Show __md_tranger__ metadata field

Search Conditions:
  --from-t TIME              Start time (e.g., "1.seconds", "2.hours")
  --to-t TIME                End time
  --from-rowid ROWID         Start row ID
  --to-rowid ROWID           End row ID
  --user-flag-set MASK       User flag mask (set)
  --user-flag-not-set MASK   User flag mask (not set)
  --system-flag-set MASK     System flag mask (set)
  --system-flag-not-set MASK System flag mask (not set)
  --key KEY                  Filter by key
  --not-key KEY              Exclude key
  --from-tm TIME             Message time range start
  --to-tm TIME               Message time range end
  --rkey RKEY                Regular expression key filter
  --filter FILTER            JSON field filter (dict string)

Print:
  --list-databases           List available databases
```

#### tr2search

Search within record content in TimeRanger2 databases. Extends `tr2list` with content-based searching: decode fields (e.g. base64), match text patterns, and display results as JSON or hexdump.

```
tr2search [OPTIONS] PATH

Database:
  -r, --recursive            List recursively through subdirectories

Presentation:
  -l, --verbose LEVEL        0=total, 1=metadata, 2=metadata+path,
                              3=metadata+record
  -m, --mode MODE            Display mode: form or table
  -f, --fields FIELDS        Show only specified fields
  -d, --show_md2             Show __md_tranger__ metadata field

Search Conditions (time):
  --from-t TIME              Start time
  --to-t TIME                End time
  --from-tm TIME             From message time
  --to-tm TIME               To message time

Search Conditions (row):
  --from-rowid ROWID         Start row ID (use -N for "last N")
  --to-rowid ROWID           End row ID

Search Conditions (flags):
  --user-flag-set MASK       User flag mask (set)
  --user-flag-not-set MASK   User flag mask (not set)
  --system-flag-set MASK     System flag mask (set)
  --system-flag-not-set MASK System flag mask (not set)

Search Conditions (keys):
  --key KEY                  Filter by key
  --not-key KEY              Exclude key

Search Conditions (content):
  --search-content-key KEY          JSON field containing data to search
  --search-content-filter FILTER    Filter to apply: clear or base64
  --search-content-text TEXT        Text to search in filtered content
  --diplay-format FORMAT            Display format: json or hexdump
```

Examples:

```bash
# Search base64-encoded frames, show last 10 records
tr2search /yuneta/store/queues/frames/myrole^myname/frames \
  --search-content-key=frame64 \
  --search-content-filter=base64 \
  --from-rowid=-10 -l3

# Search by key with content text match
tr2search /yuneta/store/queues/frames/gps^device/frames \
  --key=867060032083772 \
  --search-content-key=frame64 \
  --search-content-text="hello" -l3
```

#### tr2keys

List keys (primary keys) in TimeRanger2 topics.

```
tr2keys [OPTIONS] PATH

  -r, --recursive            List recursively
  --list-databases           List available databases
```

#### tr2migrate

Migrate data from TimeRanger v1 to TimeRanger2 format.

```
tr2migrate [OPTIONS] PATH_TRANGER PATH_TRANGER2

  -l, --verbose LEVEL        Verbose level
```

#### list_queue_msgs2

List messages in TimeRanger2-based message queues.

```
list_queue_msgs2 [OPTIONS] PATH

  -l, --verbose LEVEL        empty=total, 1=metadata, 2=metadata+record
  -a, --all                  List all messages, not only pending
  -t, --print-local-time     Display in local timezone
```

#### treedb_list

List records in TreeDB hierarchical databases built on top of TimeRanger2. A TreeDB organizes records (nodes) in a parent-child graph structure with typed schemas.

```
treedb_list [OPTIONS] PATH

Database:
  -b, --database DATABASE    TreeDB name (auto-discovered if only one exists)
  -c, --topic TOPIC          Topic name (lists all topics if omitted)
  -i, --ids ID               Specific ID or comma-separated list of IDs
  -r, --recursive            List recursively across all TreeDB databases

Presentation:
  -m, --mode MODE            Display mode: form (JSON) or table (columnar)
  -f, --fields FIELDS        Show only specified fields

Debug:
  --print-tranger            Print tranger JSON configuration
  --print-treedb             Print treedb JSON structure
```

Examples:

```bash
# List all records in default treedb
treedb_list /yuneta/store/agent/agent

# List a specific topic in table mode
treedb_list -c binaries -m table /yuneta/store/agent/agent

# List specific IDs with selected fields
treedb_list -c yunos -i "yuno1,yuno2" -f id,name,status /path/to/tranger

# Recursive listing of all treedbs under a path
treedb_list -r /yuneta/store
```

### Filesystem Monitoring

#### fs_watcher

Monitor filesystem changes using io_uring and the `fs_watcher` library. Reports file/directory creation, deletion, and modification events.

```
fs_watcher DIRECTORY
```

Watches recursively. Prints events as they occur.

#### inotify

Low-level test utility for Linux inotify filesystem event monitoring. Tracks watched directories in a JSON object. Uses io_uring for async I/O.

```
inotify DIRECTORY
```

### Code Generation

Tools for scaffolding new GClasses, yunos, and projects.

#### yuno-skeleton

Interactive generator for new yunos and GClasses from built-in templates.

```
yuno-skeleton [OPTIONS] [SKELETON]

  -l, --list                 List available skeletons
  -p, --skeletons-path PATH  Custom skeleton path (default: /yuneta/bin/skeletons)
```

Built-in skeleton templates:

| Skeleton | Type | Description |
|----------|------|-------------|
| `c_h_file` | GClass | Generic `.c` + `.h` file pair |
| `c_project` | Utility | Pure C project (argp-standalone dependency only) |
| `js_gclass` | GClass | JavaScript Yuneta GClass |
| `gclass_service` | GClass | Service GClass with timer (output events must be subscribed) |
| `gclass_child` | GClass | Child GClass (all output events forwarded to parent) |
| `yuno_standalone` | Yuno | Standalone yuno project based on yunetas kernel |
| `yuno_citizen` | Yuno | Realm citizen yuno project |

Templates use `+variable+` placeholders in filenames and `{{variable}}` in file contents. Interactive prompts ask for values like description, author, version, and license.

#### yclone-gclass

Clone and rename an existing GClass. Handles case variations (lowercase, Capitalize, UPPERCASE) in both filenames and content.

```
yclone-gclass OLD_GCLASS NEW_GCLASS
```

Example:

```bash
yclone-gclass prot_http prot_http2
# Renames c_prot_http.c → c_prot_http2.c, updates all references
```

#### yclone-project

Clone an entire Yuneta project with keyword substitution across filenames and file contents.

```
yclone-project [OPTIONS] SOURCE-PROJECT DESTINATION-PROJECT

  -s, --src-keyword KEYWORD  Source keyword to replace
  -d, --dst-keyword KEYWORD  Destination keyword
```

#### ymake-skeleton

Create a new skeleton template from an existing yuno project or GClass source.

```
ymake-skeleton [OPTIONS] SOURCE-PROJECT DESTINATION-PROJECT

  -r, --yunorole KEYWORD     YunoRole keyword
  -n, --rootname KEYWORD     RootName keyword(s), comma-separated
```

Example:

```bash
ymake-skeleton ./gate_frigo yuno_gate --yunorole=gate_frigo --rootname=gate_frigo,frigo
```

### Conversion and Validation

#### yscapec

Convert a file into an escaped C string literal. Useful for embedding binary or text data in C source.

```
yscapec [OPTIONS] FILE

  -n, --no-conversion        Don't convert double quotes to single quotes
  -s, --line-size SIZE       Output line size
```

Handles `\"`, `\\`, `\r`, `\n` escaping with configurable line wrapping.

#### ytestconfig

Validate and test a JSON configuration file against the Yuneta config parser.

```
ytestconfig [OPTIONS] FILE

  -l, --verbose              Verbose mode
```

#### pkey_to_jwks

Convert a raw base64url-encoded RSA public key (from Keycloak) to JWKS (JSON Web Key Set) format. Useful for converting legacy `Authz.jwt_public_keys` to `Authz.jwks`.

```
pkey_to_jwks --iss ISSUER BASE64_DER_RSA_KEY
```

Uses `getopt_long` (not argp). Links OpenSSL directly.

### Build Testing

#### test-musl

Minimal "Hello World" program for verifying musl static builds work correctly. No Yuneta dependencies.

```bash
# Prints: Hello from static musl binary!
```

## Implementation Patterns

### GClass-Based Tools

`ycommand`, `ybatch`, `ytests`, and `ystats` are full yuno applications with their own GClass implementations (`C_YCOMMAND`, `C_YBATCH`, `C_YTESTS`, `C_YSTATS`). They use the Yuneta entry point (`yuneta_entry_point`), event loop, and WebSocket protocol stack to communicate with remote yunos. They support OAuth2/JWT authentication.

### Standalone Tools

The remaining tools are plain C programs that may use individual Yuneta libraries (gobj, timeranger2, helpers) without the full yuno runtime. They use `argp-standalone` for command-line parsing (except `pkey_to_jwks` which uses `getopt_long`, and `inotify`/`fs_watcher` which use manual parsing).

### Common Link Libraries

All standalone tools link a subset of:

```
libargp-standalone.a       # Command-line argument parsing
libyunetas-gobj.a          # GObj framework (helpers, JSON, logging)
libjansson.a               # JSON
libpcre2-8.a               # Regular expressions
liburing.a                 # io_uring async I/O
libbacktrace.a             # Stack traces (when CONFIG_DEBUG_WITH_BACKTRACE)
```

GClass-based tools additionally link the full kernel stack:

```
libyunetas-core-linux.a    # Runtime GClasses (TCP, timers, protocols)
libtimeranger2.a           # Time-series persistence
libyev_loop.a              # Event loop
libytls.a                  # TLS abstraction
libssl.a / libcrypto.a     # OpenSSL (or mbed-TLS equivalents)
libjwt-y.a                 # JWT authentication
```
