# Plan: Improve Yunetas Tests & JSON-Driven Message Interchange Testing

## Part 1: Analysis of Current Test Infrastructure

### Current Strengths
- Memory leak detection (set_memory_check_list / get_cur_system_memory)
- Log message capture and validation (capture_log_write / set_expected_results)
- JSON comparison (test_json / test_json_file / kwid_compare_records)
- Time measurement (MT_START_TIME / MT_PRINT_TIME)
- Auto-kill timeout protection (set_auto_kill_time)
- ctest integration via CMake add_test()

### Current Weaknesses
1. **Every test requires writing C code**: Even simple "send event A, expect event B" tests need a full GClass (FSM, actions, attrs_table, create_gclass, register) + a main.c with boilerplate
2. **No event sequence validation**: Tests validate log messages, not the actual event flow between GObjects. The `capture_log_write` mechanism only checks `"msg"` strings, not event names/payloads
3. **No reusable test harness**: Each test reinvents the orchestration pattern (timer → connect → subscribe → send → verify → die)
4. **Configuration embedded in C strings**: The `variable_config` JSON is embedded as C string literals with `\n\` escaping, making them hard to read, maintain, and parameterize
5. **No parameterized tests**: Can't run the same test logic with different inputs/expected outputs without duplicating code
6. **No message trace recording**: No built-in way to record the actual sequence of events exchanged between services for later comparison

## Part 2: JSON-Driven Message Interchange Test System

### Concept

Create a **generic test runner GClass** (`C_TEST_RUNNER`) and a **generic test executor binary** (`test_msg_interchange`) that:

1. Reads a **JSON test definition file** that describes:
   - The yuno configuration (services, connections)
   - A **script** of actions to perform (send events, wait for events, check state)
   - **Expected results** (event sequence, payloads, final state)

2. Executes the script step-by-step using the standard Yunetas event loop

3. Validates results against expected outcomes and returns pass/fail via ctest

### JSON Test File Format

```json
{
    "test_name": "mqtt_qos0_pubsub",
    "test_description": "MQTT QoS 0 publish/subscribe round-trip",
    "timeout": 10,

    "variable_config": {
        "environment": { "work_dir": "/tmp" },
        "yuno": { "autoplay": true },
        "global": {
            "Authz.allow_anonymous_in_localhost": true,
            "__input_side__.__json_config_variables__": {
                "__input_url__": "mqtt://0.0.0.0:18210",
                "__input_port__": "18210"
            }
        },
        "services": [
            { "name": "authz", "gclass": "C_AUTHZ", "autostart": true, "autoplay": true },
            { "name": "mqtt_broker", "gclass": "C_MQTT_BROKER", "autostart": true, "autoplay": true,
              "kw": { "enable_new_clients": true }
            },
            {
                "name": "__input_side__", "gclass": "C_IOGATE", "autostart": true,
                "children": [
                    { "name": "server_port", "gclass": "C_TCP_S",
                      "kw": { "url": "(^^__input_url__^^)", "backlog": 4 }
                    }
                ],
                "[^^children^^]": {
                    "__range__": [1, 4],
                    "__content__": {
                        "name": "(^^__input_port__^^)-(^^__range__^^)",
                        "gclass": "C_CHANNEL",
                        "children": [
                            { "name": "(^^__input_port__^^)-(^^__range__^^)",
                              "gclass": "C_PROT_MQTT2", "kw": { "iamServer": true },
                              "children": [{ "gclass": "C_TCP" }]
                            }
                        ]
                    }
                }
            },
            {
                "name": "__output_side__", "gclass": "C_IOGATE", "autostart": false,
                "children": [
                    { "name": "mqtt_client", "gclass": "C_CHANNEL",
                      "children": [
                          { "name": "mqtt_client", "gclass": "C_PROT_MQTT2",
                            "kw": { "iamServer": false, "mqtt_client_id": "test_client_1" },
                            "children": [
                                { "name": "mqtt_client", "gclass": "C_TCP",
                                  "kw": { "url": "tcp://127.0.0.1:18210" }
                                }
                            ]
                          }
                      ]
                    }
                ]
            },
            { "name": "__top_side__", "gclass": "C_IOGATE", "autostart": false }
        ]
    },

    "script": [
        {
            "action": "start_tree",
            "target": "__output_side__",
            "comment": "Start the MQTT client connection"
        },
        {
            "action": "wait_event",
            "source": "__output_side__",
            "event": "EV_ON_OPEN",
            "timeout": 5,
            "comment": "Wait for CONNACK"
        },
        {
            "action": "send_iev",
            "target": "__output_side__",
            "event": "EV_MQTT_SUBSCRIBE",
            "kw": {
                "subs": ["test/topic"],
                "qos": 0,
                "options": 0
            },
            "comment": "Subscribe to test/topic"
        },
        {
            "action": "wait_event",
            "source": "__output_side__",
            "event": "EV_MQTT_SUBSCRIBE",
            "timeout": 5,
            "comment": "Wait for SUBACK"
        },
        {
            "action": "send_iev",
            "target": "__output_side__",
            "event": "EV_MQTT_PUBLISH",
            "kw": {
                "topic": "test/topic",
                "qos": 0,
                "retain": false,
                "payload": "Hello MQTT"
            },
            "comment": "Publish message"
        },
        {
            "action": "wait_event",
            "source": "__output_side__",
            "event": "EV_MQTT_MESSAGE",
            "timeout": 5,
            "expected_kw": {
                "topic": "test/topic",
                "payload": "Hello MQTT"
            },
            "comment": "Wait for published message to arrive and verify payload"
        },
        {
            "action": "send_event",
            "target": "__output_side__",
            "event": "EV_DROP",
            "kw": {},
            "comment": "Disconnect"
        }
    ],

    "expected_errors": [],
    "expected_event_trace": [
        {"event": "EV_ON_OPEN", "source": "__output_side__"},
        {"event": "EV_MQTT_SUBSCRIBE", "source": "__output_side__"},
        {"event": "EV_MQTT_MESSAGE", "source": "__output_side__", "kw": {"topic": "test/topic"}}
    ]
}
```

### Architecture

```
tests/c/msg_interchange/
├── CMakeLists.txt              # Builds generic test_msg_interchange binary
├── main.c                      # Generic main: reads JSON file, sets up yuno, runs
├── c_test_runner.c             # Generic GClass: executes script steps
├── c_test_runner.h
└── json_tests/                 # JSON test definitions (one per test case)
    ├── test_mqtt_qos0.json
    ├── test_tcp_echo.json
    ├── test_tcp_reconnect.json
    └── ...
```

### How it works

1. **CMakeLists.txt** registers one ctest per JSON file:
   ```cmake
   file(GLOB TEST_JSON_FILES "${CMAKE_CURRENT_SOURCE_DIR}/json_tests/*.json")
   foreach(test_file ${TEST_JSON_FILES})
       get_filename_component(test_name ${test_file} NAME_WE)
       add_test(NAME "msg_interchange/${test_name}"
                COMMAND test_msg_interchange ${test_file})
   endforeach()
   ```

2. **main.c** is generic boilerplate:
   - Reads the JSON test file from argv[1]
   - Extracts `variable_config` and builds `fixed_config` from `test_name`
   - Calls `yuneta_entry_point()` with the extracted config
   - The `register_yuno_and_more()` registers all known GClasses + C_TEST_RUNNER
   - Sets up expected_results from `expected_errors`
   - Returns pass/fail

3. **C_TEST_RUNNER** GClass:
   - Is injected as the `default_service` in the variable_config
   - On `mt_play()`: loads the script array, starts execution
   - Uses a timer to sequence steps
   - **Action types**:
     - `start_tree`: calls `gobj_start_tree()` on the named service
     - `send_event`: calls `gobj_send_event()` with the given event/kw
     - `send_iev`: wraps kw with `iev_create()` and sends via `EV_SEND_IEV`
     - `wait_event`: subscribes to the target service, waits for the specified event
     - `wait_timeout`: waits N milliseconds
     - `check_state`: verifies a service is in the expected FSM state
   - On receiving a waited event, validates `expected_kw` (if specified) using `kw_match_simple()`
   - Records all received events in a trace array
   - When script completes (or times out), validates `expected_event_trace` against the recorded trace
   - Calls `set_yuno_must_die()` to exit

### Implementation Steps

1. **Create `tests/c/msg_interchange/` directory structure**
2. **Implement `c_test_runner.h/c`** — the generic test orchestrator GClass
3. **Implement `main.c`** — generic entry point that reads JSON and runs
4. **Implement `CMakeLists.txt`** — builds binary, discovers JSON files, registers ctests
5. **Create initial JSON test files**:
   - `test_tcp_connect.json` — basic TCP connect/disconnect (equivalent to c_tcp/test1)
   - `test_tcp_echo.json` — TCP echo message (equivalent to c_tcp/test3)
   - `test_mqtt_qos0.json` — MQTT pub/sub (equivalent to c_mqtt/test1)
6. **Integrate into `tests/c/CMakeLists.txt`** — add_subdirectory for msg_interchange
7. **Build and test**

### Key Design Decisions

- **One binary, many JSON files**: Avoid compiling a new binary per test case
- **Script-based execution**: Sequential steps with wait/timeout, matching how services actually communicate
- **Reuse existing GClasses**: The runner doesn't replace C_PEPON/C_TESTON; it IS the test orchestrator that drives existing services
- **Backward compatible**: Existing tests remain unchanged; this is an additional test mechanism
- **Easy to extend**: Adding a new test = writing a JSON file, no C code needed
- **ctest native**: Each JSON file = one ctest, fully integrated with `yunetas test`
