(folders)=

# **Yuneta Directories**

The Yuneta SDK is structured into the following top folders:

- [docs](#docs):         Documentation of Yuneta.
- [kernel](#kernel):     Kernel in several languages.
- [modules](#modules):   Modules in several languages.
- [performance](#performance): Performance tests.
- [tests](#tests):       Tests.
- [tools](#tools):       Compilation or building tools.
- [utils](#utils):       Utilities for Yuneta.
- [yunos](#yunos):       Yunos supplied by the SDK.

---

(docs)=
## `docs`

- `doc.yuneta.io`: Yuneta’s Documentation built with [MyST Markdown].

---

(kernel)=
## `kernel`

The core framework of Yuneta, implemented in multiple languages.

- `C`:

    - `gobj-c`:
      G-Objects, implementation of classes and objects based on a simple Finite State Machine,
      attributes based on JSON, a comprehensive table of class methods,
      and an API to facilitate communication between objects through events,
      with an integrated publish/subscribe pattern.

    - `linux-ext-libs`:
      External libraries used by `c/root-linux`. These are statically compiled and self-contained.

    - `root-esp32`:
      Kernel for the ESP32 microcontroller based on [`esp-idf`](https://docs.espressif.com/projects/esp-idf).
      **List of components ordered by dependency (bottom = higher dependency)**:

            - esp_jansson
            - esp_gobj          (depends on esp_jansson)
            - esp_yuneta        (depends on esp_gobj)
            - esp_c_prot        (depends on esp_yuneta)

      *It is only necessary to include the component with the highest dependency.*

    - `root-linux`:
      Kernel for Linux systems.

    - `timeranger2`:
      Timeranger2, a time-series key-value database using flat files.

    - `yev_loop`:
      Library for asynchronous input/output, built on [`io_uring`](https://github.com/axboe/liburing).

    - `libjwt`:
      JWT (JSON Web Token) authentication library.

    - `ytls`:
      TLS library to manage encryption using multiple backends, such as OpenSSL and mbedTLS.

- `JS`:

    - `gobj-js`:
      Kernel implementation for JavaScript.

---

(modules)=

## `modules`

Contains additional protocol and functionality modules.

- `C`:
    - `console`: C_CONSOLE support (terminal/CLI interface).
    - `modbus`: C_MODBUS support (Modbus protocol).
    - `mqtt`: C_MQTT support (MQTT client protocol).
    - `postgres`: C_POSTGRES support (PostgreSQL database).
    - `test`: C_TEST support (testing utilities).

---

(performance)=
## `performance`

Performance testing utilities for key Yuneta components.

- `C`:
    - `perf_c_tcp`:
      Performance test of TCP-based gobjs.
    - `perf_c_tcps`:
      Performance test of TCP gobjs with encryption.
    - `perf_yev_ping_pong`:
      Performance test of the `yev_loop` library.
    - `perf_yev_ping_pong2`:
      Additional performance tests for the `yev_loop` library.

---

(tests)=
## `tests`

Tests for Yuneta components.

- `C`: C tests are created using CMake and include:
    - `c_mqtt`
    - `c_node_link_events`
    - `c_subscriptions`
    - `c_tcp`
    - `c_tcp2`
    - `c_tcps`
    - `c_tcps2`
    - `c_timer`
    - `c_timer0`
    - `kw`
    - `msg_interchange`
    - `timeranger2`
    - `tr_msg`
    - `tr_queue`
    - `tr_treedb`
    - `tr_treedb_link_events`
    - `yev_loop`

---

(tools)=
## `tools`

Auxiliary tools for building and compiling Yuneta projects.

- `cmake`:
    - Contains reusable CMake files and configurations.

---

(utils)=
## `utils`

Utility scripts and CLI tools for Yuneta.

- `C`:
    - `fs_watcher`: Monitors filesystem changes.
    - `inotify`: Tracks file events using inotify.
    - `json_diff`: JSON diff utility.
    - `list_queue_msgs2`: Lists queue messages.
    - `msg2db_list`: Lists msg2db records.
    - `pkey_to_jwks`: Converts public keys to JWKS format.
    - `stats_list`: Lists statistics.
    - `test-static`: Tests for static builds.
    - `time2date`: Converts timestamps to dates.
    - `time2range`: Converts timestamps to time ranges.
    - `tr2keys`: Processes keys in Timeranger2.
    - `tr2list`: Lists entries in Timeranger2.
    - `tr2migrate`: Migrates data between Timeranger2 instances.
    - `tr2search`: Searches in Timeranger2.
    - `treedb_list`: Lists TreeDB entries.
    - `ybatch`: Batch operations for yunos.
    - `yclone-gclass`: Clones a GClass template.
    - `yclone-project`: Clones a project template.
    - `ycommand`: Control-plane CLI for running yunos.
    - `ylist`: Lists yunos.
    - `ymake-skeleton`: Generates project skeletons.
    - `yscapec`: Escapes C strings.
    - `yshutdown`: Shuts down yunos.
    - `ystats`: Retrieves yuno statistics.
    - `ytestconfig`: Tests configuration files.
    - `ytests`: Test runner.
    - `yuno-skeleton`: Generates yuno skeletons.

---

(yunos)=
## `yunos`

Pre-supplied full applications built with Yuneta.

- `C`:
    - `auth_bff`: Authentication backend-for-frontend.
    - `controlcenter`: Control center service.
    - `dba_postgres`: PostgreSQL database agent.
    - `emailsender`: Email sending service.
    - `emu_device`: Device emulator.
    - `logcenter`: Log aggregation center.
    - `mqtt_broker`: MQTT v3.1.1 + v5.0 broker with persistence.
    - `mqtt_tui`: MQTT text-based UI.
    - `sgateway`: Service gateway.
    - `watchfs`: Filesystem watcher service.
    - `yuno_agent`: Yuno lifecycle manager (start/stop/update).
    - `yuno_agent22`: Yuno agent v2.
    - `yuno_cli`: Yuno command-line interface.

- `JS`:
    - `gui_yunetas.js`: Graphical User Interface (GUI) for managing Yuneta.


[MyST Markdown]: https://mystmd.org/guide
