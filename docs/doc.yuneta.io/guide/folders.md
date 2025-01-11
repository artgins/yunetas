# Folders

The Yuneta SDK is structured in next top folders:

- [](#docs):         Documentation of Yunetas
- [](#kernel):       Kernel in several languages
- [](#modules):      Modules in several languages
- [](#performance)   Performance tests
- [](#tests)         Tests
- [](#tools)         Compilation or building tools
- [](#utils)         Utilities for Yunetas
- [](#yunos)         Yunos


(docs)=
## `docs`

- `doc.yuneta.io`: YunetaS Documentation built with [sphinx],  [sphinx-book-theme] and [MyST Markdown].


(kernel)=
## `kernel`

- `C`:

    - `gobj-c`:
        G-Objects, implementation of classes and objects based in a simple Finite State Machine,
        attributes based in json, a wide table of class methods,
        and an api to let communicate between objects through events,
        with publish/subscribe pattern integrated.

    - `linux-ext-libs`:
        External libraries used by `c/root-linux`, static and self compiled.

    - `root-esp32`:
        Kernel for esp32 microcontroller [`esp-idf`](https://docs.espressif.com/projects/esp-idf).
        List of components order by dependency (bottom higher dependency) :

            - esp_jansson
            - esp_gobj          (depends of esp_jansson)
            - esp_yuneta        (depends of esp_gobj)
            - esp_c_prot        (depends of esp_yuneta)

        It's only necessary to include the component with higher dependency

    - `root-linux`:
        Kernel for linux.

    - `timeranger2`:
        Timeranger2, a serie-time key-value database over flat files.

    - `yev_loop`:
        Library for asynchronous input/output based in [`io_uring`](https://github.com/axboe/liburing). 
    - `ytls`:
        TLS library to manage encryption using several libraries: openssl, mbedtls.


- `JS`:
    - `gobj-js`:
        Kernel for javascript


(modules)=
## `modules`

- `C`:
    - `c_prot`:
        Module with several protocols


(performance)=
## `performance`

- `C`:
    - `perf_c_tcp`:
        Performance test of tcp gobjs.
    - `perf_c_tcps`:
        Performance test of tcp gobjs with encryption.
    - `perf_yev_ping_pong`:
        Performance test of yev_loop library
    - `perf_yev_ping_pong2`:
        Other performance test of yev_loop library


(tests)=
## `tests`

Tests.

- `C`: C tests are made with CMake.

    - `c_tcp`:
    - `c_tcps`:
    - `c_timer`:
    - `c_timer0`:
    - `timeranger2`:
    - `tr_msg`:
    - `tr_treedb`:
    - `yev_loop`:


(tools)=
## `tools`


- `cmake`
    Auxiliary tools: cmake files,...


(utils)=
## `utils`

Utilities (CLI's)

- `C`:
    - `fs_watcher`
    - `inotify`
    - `tr2keys`
    - `tr2list`
    - `tr2migrate`


(yunos)=
## `yunos`

Full applications or utilities built with yunetas

- `gui_yunetas.js`
    GUI of Yunetas.
- `tui_yunetas.py`
    TUI of Yunetas.


[sphinx]:   https://www.sphinx-doc.org/
[sphinx-book-theme]: https://sphinx-book-theme.readthedocs.io/en/stable/
[MyST Markdown]: https://mystmd.org/guide
