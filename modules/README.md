# modules

Optional GClass libraries that extend the Yuneta kernel with domain-specific functionality. Each module is conditionally compiled based on `.config` Kconfig options and produces a static library linked by yunos and utilities that need it.

## Directory Layout

```
modules/
├── c/
│   ├── console/        # Terminal UI: line editing with ncurses (C_EDITLINE)
│   ├── mqtt/           # MQTT v3.1/v3.1.1/v5.0 protocol and broker (C_PROT_MQTT2, C_MQTT_BROKER)
│   ├── postgres/       # PostgreSQL async client (C_POSTGRES)
│   └── test/           # Test GClasses for traffic testing (C_PEPON, C_TESTON)
└── js/                 # JavaScript libraries (w2ui fork, deprecated)
```

## Module Summary

| Module | Kconfig option | Library | GClasses |
|--------|---------------|---------|----------|
| console | `CONFIG_MODULE_CONSOLE` | `libyunetas-module-console.a` | `C_EDITLINE` |
| mqtt | `CONFIG_MODULE_MQTT` | `libyunetas-module-mqtt.a` | `C_PROT_MQTT2`, `C_MQTT_BROKER` |
| postgres | `CONFIG_MODULE_POSTGRES` | `libyunetas-module-postgres.a` | `C_POSTGRES` |
| test | `CONFIG_MODULE_TEST` | `libyunetas-module-test.a` | `C_PEPON`, `C_TESTON` |

All modules default to enabled (`default y` in Kconfig). Disable any module with `menuconfig` or by editing `.config`.

---

## Console Module

Interactive terminal line-editing with ncurses. Provides the `C_EDITLINE` GClass used by `ycommand` for its interactive mode.

### Source Files

| File | Purpose |
|------|---------|
| `c_editline.c/.h` | `C_EDITLINE` GClass — line editor with history, completion, scrolling |
| `linenoise.c/.h` | Embedded [linenoise](https://github.com/antirez/linenoise) library (line editing engine) |
| `help_ncurses.c/.h` | Ncurses helper: window open/close, color management |
| `g_ev_console.c/.h` | Global event declarations for console GClasses |
| `g_st_console.c/.h` | Global state declarations for console GClasses |

### C_EDITLINE GClass

An ncurses-based line editor with command history, key bindings, and color support.

**Registration:** `register_c_editline()`

**Attributes:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `use_ncurses` | boolean | false | Enable ncurses mode |
| `prompt` | string | `"> "` | Command prompt |
| `history_file` | string | — | Path to persistent history file |
| `history_max_len` | integer | 200000 | Maximum history lines |
| `buffer_size` | integer | 4096 | Edition buffer size |
| `x`, `y` | integer | 0 | Window position |
| `cx`, `cy` | integer | 80, 1 | Window dimensions |
| `bg_color` | string | `"cyan"` | Background color |
| `fg_color` | string | `"white"` | Foreground color |

**FSM:** Single state `ST_IDLE`.

**Events (declared in `g_ev_console.h`):**

Editline operations:
`EV_KEYCHAR`, `EV_MOUSE`, `EV_EDITLINE_ENTER`, `EV_EDITLINE_BACKSPACE`, `EV_EDITLINE_DEL_CHAR`, `EV_EDITLINE_MOVE_START`, `EV_EDITLINE_MOVE_END`, `EV_EDITLINE_MOVE_LEFT`, `EV_EDITLINE_MOVE_RIGHT`, `EV_EDITLINE_PREV_HIST`, `EV_EDITLINE_NEXT_HIST`, `EV_EDITLINE_COMPLETE_LINE`, `EV_EDITLINE_DEL_EOL`, `EV_EDITLINE_SWAP_CHAR`, `EV_EDITLINE_DEL_LINE`, `EV_EDITLINE_DEL_PREV_WORD`

Window management:
`EV_PAINT`, `EV_SIZE`, `EV_MOVE`, `EV_SETFOCUS`, `EV_KILLFOCUS`, `EV_SET_TOP_WINDOW`, `EV_SCREEN_SIZE_CHANGE`, `EV_PREVIOUS_WINDOW`, `EV_NEXT_WINDOW`

Scrolling:
`EV_SCROLL_PAGE_UP`, `EV_SCROLL_PAGE_DOWN`, `EV_SCROLL_LINE_UP`, `EV_SCROLL_LINE_DOWN`, `EV_SCROLL_TOP`, `EV_SCROLL_BOTTOM`

Text and UI:
`EV_COMMAND`, `EV_GETTEXT`, `EV_SETTEXT`, `EV_CLRSCR`, `EV_CLEAR_HISTORY`, `EV_QUIT`, `EV_SET_SELECTED_BUTTON`, `EV_GET_PREV_SELECTED_BUTTON`, `EV_GET_NEXT_SELECTED_BUTTON`

**Helper API:**

```c
int tty_keyboard_init(void);   // Create stdin fd for keyboard input (no echo)

WINDOW *open_ncurses(hgobj gobj);
void close_ncurses(void);
int get_paint_color(const char *fg_color, const char *bg_color);
```

---

## MQTT Module

Full MQTT protocol implementation supporting v3.1, v3.1.1, and v5.0. Provides both the wire protocol handler (`C_PROT_MQTT2`) and the broker logic (`C_MQTT_BROKER`). Based on Mosquitto code (EPL-2.0 / BSD-3-Clause), refactored for the Yuneta GClass architecture.

### Source Files

| File | Purpose |
|------|---------|
| `c_prot_mqtt2.c/.h` | `C_PROT_MQTT2` GClass — MQTT wire protocol (client and server side) |
| `c_mqtt_broker.c/.h` | `C_MQTT_BROKER` GClass — broker logic, session management, subscriptions |
| `mqtt_util.c/.h` | MQTT constants, enums, validation, and utility functions |
| `tr2q_mqtt.c/.h` | Persistent MQTT message queues backed by TimeRanger2 |
| `treedb_schema_mqtt_broker.c` | TreeDB schema for broker persistent data model |

### C_PROT_MQTT2 GClass

Handles the MQTT wire protocol: packet framing, encoding/decoding, property parsing (v5), QoS flow, and keep-alive. Works as either client or server side depending on the `iamServer` attribute.

**Registration:** `register_c_prot_mqtt2()`

**FSM States:**

| State | Description |
|-------|-------------|
| `ST_DISCONNECTED` | Not connected |
| `ST_WAIT_HANDSHAKE` | TCP connected, waiting for CONNECT/CONNACK |
| `ST_WAIT_FRAME_HEADER` | In session, waiting for next packet header |
| `ST_WAIT_PAYLOAD` | Header received, waiting for payload bytes |

**Events (output):**

| Event | Description |
|-------|-------------|
| `EV_MQTT_PUBLISH` | Outgoing publish acknowledged (QoS 0: sent, QoS 1: PUBACK, QoS 2: PUBCOMP) |
| `EV_MQTT_MESSAGE` | Incoming message received with QoS flow completed |
| `EV_MQTT_SUBSCRIBE` | SUBACK received in response to SUBSCRIBE |
| `EV_MQTT_UNSUBSCRIBE` | UNSUBACK received in response to UNSUBSCRIBE |

**Key Attributes:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `iamServer` | boolean | false | Server or client side |
| `tranger_queues` | pointer | — | TimeRanger for persistent QoS queues |
| `treedb_name` | string | `"treedb_mqtt_broker"` | TreeDB service for auth |
| `mqtt_client_id` | string | — | Client identifier |
| `mqtt_protocol` | string | `"mqttv5"` | Protocol version (`mqttv5`, `mqttv311`, `mqttv31`) |
| `mqtt_clean_session` | string | `"1"` | Clean session flag |
| `mqtt_session_expiry_interval` | string | `"-1"` | Session expiry (seconds, -1 = never) |
| `mqtt_keepalive` | string | `"60"` | Keep-alive interval |
| `mqtt_will_topic` | string | — | Last Will topic |
| `mqtt_will_payload` | string | — | Last Will payload |
| `mqtt_will_qos` | string | — | Last Will QoS |
| `mqtt_will_retain` | string | — | Last Will retain flag |
| `mqtt_connect_properties` | string | — | v5 CONNECT properties (JSON) |
| `mqtt_will_properties` | string | — | v5 Will properties (JSON) |
| `backup_queue_size` | integer | 60000 | Queue backup threshold |
| `alert_queue_size` | integer | 2000 | Queue alert threshold |
| `timeout_ack` | integer | 60 | ACK timeout (seconds) |
| `timeout_handshake` | integer | 5000 | Handshake timeout (ms) |
| `max_inflight_messages` | integer | 20 | Max concurrent QoS 1/2 transmissions |
| `max_queued_messages` | integer | 1000 | Max queued messages per client |
| `max_qos` | integer | 2 | Maximum allowed QoS level |
| `retain_available` | boolean | true | Allow retained messages |
| `max_keepalive` | integer | 60 | Server keep-alive override (v5) |
| `max_topic_alias` | integer | 10 | Max topic aliases (v5) |

**Commands:** `help`

### C_MQTT_BROKER GClass

Broker-level logic: client registration, subscription tree management, message routing, retained message storage, will message handling, session persistence, and alarms.

**Registration:** `register_c_mqtt_broker()`

**FSM:** Single state `ST_IDLE`.

**Key Attributes:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `enable_new_clients` | boolean | false | Auto-create clients not in DB |
| `mqtt_persistent_db` | boolean | true | Persistent database for sessions |
| `mqtt_service` | string | — | Service name (default: yuno_role) |
| `mqtt_tenant` | string | — | Multi-tenant identifier (default: yuno_name) |
| `use_internal_schema` | boolean | true | Use hardcoded TreeDB schema |
| `deny_subscribes` | json | — | JSON list of denied topic filters |
| `on_critical_error` | integer | 2 | Error behavior (2 = exit zero) |
| `timeout` | integer | 1000 | Periodic timer interval (ms) |

**Commands:**

| Command | Description |
|---------|-------------|
| `help` | Command help |
| `list-channels` | List input channels of connected devices |
| `list-sessions` | List active sessions |
| `list-queues` | List message queues |
| `normal-subs` | List normal subscribers |
| `shared-subs` | List shared subscribers |
| `flatten-subs` | Flatten subscription tree |
| `list-retains` | List retained messages (use `#` as `/`) |
| `remove-retains` | Remove retained messages |
| `clean-queues` | Clean non-persistent, unused queues and sessions |
| `authzs` | Authorization help |

### TreeDB Schema (`treedb_mqtt_broker`)

Persistent data model with the following topics:

| Topic | PKey | Description |
|-------|------|-------------|
| `client_groups` | `id` | Hierarchical client group tree with managers |
| `clients` | `id` (client_id) | Persistent client identity and settings |
| `client_types` | `id` | Client type templates with inherited settings |
| `sessions` | `id` (client_id) | Session state: subscriptions, will, last_mid |
| `retained_msgs` | `id` (topic) | Retained messages by topic |
| `users` | `id` | User accounts with group memberships |

Alarms schema (`msg2db_alarms`): time-series alarm records per client.

### MQTT Utility Functions (`mqtt_util.h`)

```c
// String conversions
const char *mqtt_connack_string(mqtt311_connack_codes_t connack_code);
const char *mqtt_reason_string(mqtt5_reason_codes_t reason_code);
const char *mqtt_command_string(mqtt_message_t command);
const char *mqtt_property_identifier_to_string(uint32_t identifier);

// Topic validation
int mqtt_pub_topic_check2(const char *topic, size_t topiclen);
int mqtt_sub_topic_check2(const char *topic, size_t topiclen);
int mqtt_validate_utf8(const char *str, int len);

// Message construction
json_t *new_mqtt_message(hgobj gobj, const char *client_id, const char *topic,
    gbuffer_t *gbuf_payload, uint8_t qos, uint16_t mid, BOOL retain, BOOL dup,
    json_t *properties, uint32_t expiry_interval, json_int_t t);

time_t mosquitto_time(void);
```

Defines all MQTT protocol constants: message types (`CMD_CONNECT` through `CMD_AUTH`), v3.1.1 CONNACK codes, v5 reason codes, v5 property identifiers, v5 subscription options, and protocol version constants.

### Persistent Queue API (`tr2q_mqtt.h`)

TimeRanger2-backed message queues with inflight/queued separation and MQTT-specific metadata encoded in user flags.

```c
// Queue lifecycle
tr2_queue_t *tr2q_open(json_t *tranger, const char *topic_name, const char *tkey,
    system_flag2_t system_flag, size_t max_inflight_messages, size_t backup_queue_size);
void tr2q_close(tr2_queue_t *trq);

// Message operations
q2_msg_t *tr2q_append(tr2_queue_t *trq, json_int_t t, json_t *kw, uint16_t user_flag);
int tr2q_unload_msg(q2_msg_t *msg, int32_t result);
json_t *tr2q_msg_json(q2_msg_t *msg);
q2_msg_t *tr2q_get_by_rowid(tr2_queue_t *trq, uint64_t rowid);
q2_msg_t *tr2q_get_by_mid(tr2_queue_t *trq, json_int_t mid);
int tr2q_move_from_queued_to_inflight(q2_msg_t *msg);

// Loading
int tr2q_load(tr2_queue_t *trq);              // Load pending messages
int tr2q_load_all(tr2_queue_t *trq, int64_t from_rowid, int64_t to_rowid);
int tr2q_load_all_by_time(tr2_queue_t *trq, int64_t from_t, int64_t to_t);

// Iteration
q2_msg_t *tr2q_first_inflight_msg(tr2_queue_t *trq);
q2_msg_t *tr2q_first_queued_msg(tr2_queue_t *trq);
q2_msg_t *tr2q_next_msg(q2_msg_t *msg);
size_t tr2q_inflight_size(tr2_queue_t *trq);
size_t tr2q_queued_size(tr2_queue_t *trq);

// Maintenance
int tr2q_check_backup(tr2_queue_t *trq);
int tr2q_list_msgs(tr2_queue_t *trq);
int tr2q_save_hard_mark(q2_msg_t *msg, uint16_t value);
```

**Iteration macros:**
```c
Q2MSG_FOREACH_FORWARD_INFLIGHT(trq, msg)
Q2MSG_FOREACH_FORWARD_INFLIGHT_SAFE(trq, msg, next)
Q2MSG_FOREACH_FORWARD_QUEUED(trq, msg)
Q2MSG_FOREACH_FORWARD_QUEUED_SAFE(trq, msg, next)
```

**User flag bitmask layout** (16-bit `user_flag` in TimeRanger2 metadata):

```
Bits 15-12: State     (mosq_ms_publish_qos0 .. mosq_ms_queued)
Bits 11-10: Origin    (mosq_mo_client, mosq_mo_broker)
Bits  9-8:  Direction (mosq_md_in, mosq_md_out)
Bit     7:  Dup
Bit     6:  Retain
Bits  5-4:  QoS       (0, 1, 2)
Bits  3-1:  Reserved
Bit     0:  Pending   (TR2Q_MSG_PENDING)
```

---

## Postgres Module

Asynchronous PostgreSQL client GClass using libpq non-blocking API, integrated with the Yuneta event loop.

### Source Files

| File | Purpose |
|------|---------|
| `c_postgres.c/.h` | `C_POSTGRES` GClass — async PostgreSQL queries |

### C_POSTGRES GClass

**Registration:** `register_c_postgres()`

**FSM States:**

| State | Description |
|-------|-------------|
| `ST_DISCONNECTED` | Not connected to PostgreSQL |
| `ST_WAIT_CONNECTED` | Connection in progress (non-blocking) |
| `ST_CONNECTED` | Connected and ready for queries |
| `ST_WAIT_DISCONNECTED` | Disconnection in progress |

**Events (input):**

| Event | Description |
|-------|-------------|
| `EV_SEND_QUERY` | Send a SQL query |
| `EV_CLEAR_QUEUE` | Clear pending query queue |

**Key Attributes:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `schema` | json | — | Database schema definition |
| `url` | string | — | PostgreSQL connection URL (persistent, writable) |
| `manual` | boolean | false | Manual connection mode |
| `timeout_waiting_connected` | integer | 10000 | Connection timeout (ms) |
| `timeout_between_connections` | integer | 5000 | Retry interval (ms) |
| `timeout_response` | integer | 10000 | Query response timeout (ms) |

**Commands:**

| Command | Description |
|---------|-------------|
| `help` | Command help |
| `authzs` | Authorization help |
| `list-size` | Size of queue messages |
| `list-queue` | List queue messages |
| `view-channels` | View channel messages |

**System dependency:** `sudo apt-get install postgresql-server-dev-all libpq-dev`

---

## Test Module

GClasses for traffic testing between client and server yunos. Used by the test suites in `tests/c/`.

### Source Files

| File | Purpose |
|------|---------|
| `c_pepon.c/.h` | `C_PEPON` GClass — test server (echo service) |
| `c_teston.c/.h` | `C_TESTON` GClass — test client (traffic generator) |

### C_PEPON GClass

Server-side test GClass that echoes received messages back to the sender.

**Registration:** `register_c_pepon()`

**FSM:** Single state `ST_IDLE`.

**Key Attributes:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `do_echo` | boolean | true | Echo received messages |
| `timeout` | integer | 2000 | Timer interval (ms) |

**Commands:** `help`, `authzs`

### C_TESTON GClass

Client-side test GClass that generates traffic for testing.

**Registration:** `register_c_teston()`

**FSM:** Single state `ST_IDLE`.

**Events (input):**

| Event | Description |
|-------|-------------|
| `EV_START_TRAFFIC` | Begin generating test traffic |

**Key Attributes:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `timeout` | integer | 2000 | Timer interval (ms) |

**Commands:** `help`, `authzs`

---

## Build Details

Each module follows the same conditional compilation pattern:

```cmake
if(CONFIG_MODULE_<NAME>)
    add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
    install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})
    install(TARGETS ${PROJECT_NAME} DESTINATION ${LIB_DEST_DIR})
else()
    install(CODE "message(STATUS \"Nothing to install\")")
endif()
```

When a module is disabled in `.config`, nothing is compiled and its library is not produced. Executables that reference the module's `${*_LIBS}` variable from `project.cmake` will get an empty string, so no link errors occur.

### Link Variables (from `project.cmake`)

| Config option | CMake variable | Library |
|---------------|---------------|---------|
| `CONFIG_MODULE_CONSOLE` | `${MODULE_CONSOLE}` | `libyunetas-module-console.a` |
| `CONFIG_MODULE_MQTT` | `${MODULE_MQTT}` | `libyunetas-module-mqtt.a` |
| `CONFIG_MODULE_POSTGRES` | `${MODULE_POSTGRES}` | `libyunetas-module-postgres.a` |
| `CONFIG_MODULE_TEST` | `${MODULE_TEST}` | `libyunetas-module-test.a` |

---

## JavaScript Modules

The `modules/js/` directory previously held a fork of [w2ui](https://github.com/vitmalina/w2ui) with added window management. This is now deprecated in favor of the JavaScript implementation in `kernel/js/yunetas-js7/`.
