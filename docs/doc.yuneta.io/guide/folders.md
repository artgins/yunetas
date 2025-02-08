(folders())=
# **Yuneta Directories**

The Yuneta SDK is structured into the following top folders:

- [docs](docs):         Documentation of Yuneta.
- [kernel](kernel):     Kernel in several languages.
- [modules](modules):   Modules in several languages.
- [performance](performance): Performance tests.
- [tests](tests):       Tests.
- [tools](tools):       Compilation or building tools.
- [utils](utils):       Utilities for Yuneta.
- [yunos](yunos):       Yunos supplied by the SDK.

---

(docs())=
## `docs`

- `doc.yuneta.io`: Yuneta’s Documentation built with:
    - [Sphinx]
    - [Sphinx-Book-Theme]
    - [MyST Markdown]

---

(kernel())=
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

    - `ytls`:
      TLS library to manage encryption using multiple backends, such as OpenSSL and mbedTLS.

- `JS`:

    - `gobj-js`:
      Kernel implementation for JavaScript.

---

(modules())=
## `modules`

Contains additional protocol and functionality modules.

- `C`:
    - `c_prot`:
      Module with several communication protocols.

---

(performance())=
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

(tests())=
## `tests`

Tests for Yuneta components.

- `C`: C tests are created using CMake and include:
    - `c_tcp`
    - `c_tcps`
    - `c_timer`
    - `c_timer0`
    - `timeranger2`
    - `tr_msg`
    - `tr_treedb`
    - `yev_loop`

---

(tools())=
## `tools`

Auxiliary tools for building and compiling Yuneta projects.

- `cmake`:
    - Contains reusable CMake files and configurations.

---

(utils())=
## `utils`

Utility scripts and CLI tools for Yuneta.

- `C`:
    - `fs_watcher`: Monitors filesystem changes.
    - `inotify`: Tracks file events using inotify.
    - `tr2keys`: Processes keys in Timeranger2.
    - `tr2list`: Lists entries in Timeranger2.
    - `tr2migrate`: Migrates data between Timeranger2 instances.

---

(yunos())=
## `yunos`

Pre-supplied full applications or utilities built with Yuneta.

- `gui_yunetas.js`:
  Graphical User Interface (GUI) for managing Yuneta.

- `tui_yunetas.py`:
  Text-based User Interface (TUI) for managing Yuneta.


[sphinx]: https://www.sphinx-doc.org/
[sphinx-book-theme]: https://sphinx-book-theme.readthedocs.io/en/stable/
[MyST Markdown]: https://mystmd.org/guide
