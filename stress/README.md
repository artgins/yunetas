# Stress Tests

Stress-testing tools for evaluating Yuneta's TCP server scalability under high connection counts and sustained message traffic. The suite includes a C server binary (`stress_listen`) and two Node.js client generators (`stress-connections.js`, `stress-traffic.js`).

## stress_listen -- TCP Connection and Traffic Stress Server

**Binary:** `stress_listen`
**GClass:** `C_LISTEN`

A Yuneta yuno that listens for TCP connections on a configurable port and tracks connection/message metrics. Designed to accept massive numbers of concurrent connections (tested up to 1.5 million) while reporting real-time throughput and CPU usage.

### Architecture

```
C_LISTEN (root service -- monitoring, commands, stats)
|
+-- C_IOGATE (__input_side__)
    |
    +-- C_TCP_S (server_port, tcp://0.0.0.0:7779, backlog=32767)
        |
        +-- [11,000 pre-allocated C_CHANNEL children]
            |
            +-- C_PROT_TCP4H (4-byte length-header framing)
                |
                +-- C_TCP (individual socket)
```

### How It Works

1. On startup, pre-allocates 11,000 `C_CHANNEL` -> `C_PROT_TCP4H` -> `C_TCP` connection slots.
2. On `mt_play`, subscribes to all events from `__input_side__` and starts the TCP server tree.
3. Each accepted connection fires `EV_ON_OPEN`; the handler increments a global counter and prints throughput every 10,000 connections.
4. Incoming messages (`EV_ON_MESSAGE`) are decoded by `C_PROT_TCP4H` and delivered to `C_LISTEN` (currently a sink -- messages are read but not echoed).
5. Exposes `mt_stats()` returning connection stats from `C_IOGATE` and live CPU usage.

### FSM

Single state `ST_IDLE` handling:

| Event | Action |
|-------|--------|
| `EV_ON_OPEN` | Increment connection counter, print throughput every 10k |
| `EV_ON_MESSAGE` | Decode message (optional trace dump) |
| `EV_ON_CLOSE` | No-op |
| `EV_STOPPED` | Destroy volatile children |
| `EV_TIMEOUT` | No-op |

### Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `reset-connections` | Reset the connection counter to 0 |

### Configuration Defaults

| Parameter | Value |
|-----------|-------|
| Listen URL | `tcp://0.0.0.0:7779` |
| Backlog | 32,767 |
| Pre-allocated channels | 11,000 |
| io_uring entries | 32,768 |
| Open files limit | 200,000 |
| CPU affinity | Core 4 |
| Max system memory | 16 GB |
| Max block size | 200 MB |
| Log file | `stress-listen-W.log` |
| UDP log target | `udp://127.0.0.1:1992` |

### Source Files

| File | Purpose |
|------|---------|
| `main_listen.c` | Yuno entry point, fixed/variable config, memory limits |
| `c_listen.c` | C_LISTEN GClass -- FSM, commands, stats, connection tracking |
| `c_listen.h` | GClass declaration |

## Client Generators (Node.js)

### stress-connections.js -- Connection Load Generator

Opens N concurrent TCP connections to measure connection acceptance rate, with optional CPU load injection and periodic disconnect/reconnect cycles.

```
Usage: stress-connections.js [options]

Options:
  -h, --host         Target host (default: 127.0.0.1)
  -p, --port         Target port (default: 7779)
  -c, --connections  Number of concurrent connections (default: 10)
  -j, --jobs         CPU load per connection (console.log loop iterations, default: 0)
  -d, --disconnect   Seconds before closing each connection (default: 60, 0=no-disconnect)
      --help         Show help
```

When `-d > 0`, closed connections are immediately reopened, creating sustained connect/disconnect pressure. Per-connection and aggregate ops/sec are printed.

### stress-traffic.js -- Message Traffic Generator

Opens N concurrent connections and sends messages at a configurable rate per connection. Messages use the `C_PROT_TCP4H` wire format: a 4-byte big-endian length header followed by a JSON payload.

```
Usage: stress-traffic.js [options]

Options:
  -h, --host         Target host (default: 127.0.0.1)
  -p, --port         Target port (default: 7779)
  -c, --connections  Number of concurrent connections (default: 10)
  -r, --rate         Messages per second per connection (can be < 1, default: 1)
  -n, --count        Max messages per connection (0 = unlimited, default: 0)
  -m, --message      JSON message string to send
  -f, --msgfile      Path to JSON file to load message from
      --help         Show help
```

Each connection gets a unique hex device ID (`DVES_XXXXXX`). The default message simulates IoT telemetry (~300 bytes of sensor data). Closed connections automatically reconnect after 5 seconds.

**Effective throughput** = `connections x rate`. For example, `-c 5000 -r 10` produces 50,000 msg/sec.

### Prerequisites

```bash
# Install Node.js via nvm
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
export NVM_DIR="$HOME/.nvm" && . "$NVM_DIR/nvm.sh"
nvm install --lts && nvm alias default lts/*

# Install dependencies
cd stress/c/listen
npm install

# Raise open file limit (needed for large connection counts)
ulimit -n 200000
```

## Recorded Results

All results measured on the developer's machines (single core):

### Connection Acceptance (22-May-2025)

| Connections | Elapsed | Throughput |
|-------------|---------|------------|
| 10,000 | 0.35 sec | 28,388 ops/sec |
| 30,000 | 2.45 sec | 12,235 ops/sec |

### Message Throughput (16-Jun-2025)

| Setup | Throughput | CPU |
|-------|-----------|-----|
| OVH server, 1 core | 50,000 msg/sec | 76% |
| Laptop, 1 core | 80,000 msg/sec | 70% |
| Laptop, 1 core | 100,000 msg/sec | 95% |
| 2 x (`-c 5000 -r 10`), Clang release | 100,000 msg/sec | 87% |
| 2 x (`-c 5000 -r 9`) | 90,000 msg/sec | 80% |

### Memory Usage (concurrent connections, idle)

| Connections | Internal memory | Total (OS) |
|-------------|----------------|------------|
| 10 | 0.4 MB | 0.8 MB |
| 1,000 | 24 MB | 32 MB |
| 10,000 | 238 MB | 319 MB |
| 30,000 | 715 MB | 955 MB |
| 50,000 | 1.19 GB | 1.6 GB |
| 100,000 | 2.38 GB | 3.2 GB |
| 1,310,000 | 2.9 GB | 4.2 GB |
| 1,500,000 | 3.53 GB | 4.8 GB |

### Compiler Comparison (23-Jun-2025, 15-Jul-2025)

Full `ctest` results across compiler/build-type combinations are archived in:
- `results-2025-06-23/` -- Clang, GCC, and Musl in prod and debug modes
- `results-2025-07-15/` -- GCC debug/release/relwithdebinfo, Clang release, Musl release

## Deployment

### Manual (via ycommand)

```bash
cd stress/c/listen
ycommand -c 'install-binary id=stress_listen content64=$$(stress_listen)'
ycommand -c 'create-config id=stress_listen.10 content64=$$(./stress_listen.10.json)'
ycommand -c 'create-yuno id=10 realm_id=utilities.artgins.com yuno_role=stress_listen yuno_name=10 must_play=1'
```

### Automated (via deploy scripts)

```bash
cd stress/c/listen/deploy-yuno
bash create-stress.sh   # shuts down agent, resets DB, creates realm, deploys all yunos
```

The `_stress-test.json` batch file deploys `emailsender`, `logcenter`, and `stress_listen` into the `utilities.artgins.com` realm.

## Debugging

```bash
# CPU, file descriptors, and disk I/O per process
pidstat -p <pid> 1 -u -v -d -h

# System calls
sudo strace -p <pid>
sudo strace -p <pid> -k   # with stack trace

# Real-time stats (every 10 seconds)
./view-stats.sh

# Reset connection counter
ycommand -c 'reset-connections'
```

## Directory Structure

```
stress/c/
  CMakeLists.txt                          # adds listen/ subdirectory
  listen/
    CMakeLists.txt                        # builds stress_listen binary
    main_listen.c                         # yuno entry point and config
    c_listen.c                            # C_LISTEN GClass implementation
    c_listen.h                            # GClass declaration
    stress-connections.js                 # Node.js connection load generator
    stress-traffic.js                     # Node.js message traffic generator
    package.json                          # Node.js dependencies (minimist)
    listen.sh                             # launch stress_listen with config files
    view-stats.sh                         # view real-time stats via ystats
    deploy-yuno/
      create-stress.sh                    # full deployment: reset agent, create realm, deploy
      _deploy_all.sh                      # run ybatch with _stress-test.json
      _stress-test.json                   # batch commands: deploy emailsender, logcenter, stress_listen
      emailsender.artgins.json            # emailsender service config
      logcenter.artgins.json              # logcenter service config
      stress_listen.10.json               # stress_listen yuno config
    results-2025-06-23/                   # ctest results: clang/gcc/musl x prod/debug
    results-2025-07-15/                   # ctest results: gcc variants, clang release, musl release
```
