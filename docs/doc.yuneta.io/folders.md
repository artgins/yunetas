Directories
===========

Top folders:
    - docs
    - kernel
    - modules
    - performance
    - scripts
    - tests
    - tools
    - yunos

Folder docs
----------

Folder with documentations.

doc.yuneta.io: YunetaS Documentation implemented with sphinx.

Folder kernel
-------------

- c/gobj-c:
    G-Objects, implementation of classes and objects based in a simple Finite State Machine,
    attributes based in json, a wide table of class methods,
    and an api to let communicate between objects through events,
    with publish/subscribe pattern integrated.

- c/root-esp32:
    Kernel for esp32 microcontroller (`esp-idf <https://docs.espressif.com/projects/esp-idf/>`_).
    List of components order by dependency (bottom higher dependency) ::
        - esp_jansson
        - esp_gobj          (depends of esp_jansson)
        - esp_yuneta        (depends of esp_gobj)
        - esp_c_prot        (depends of esp_yuneta)

    It's only necessary to include the component with higher dependency

- c/root-linux:
    Kernel for linux, based in `io_uring <https://github.com/axboe/liburing>`_.

- c/root-linux-ext-libs:
    External libraries used by root-linux-c, static and self compiled.

- js/gobj-js
    Kernel for javascript

Folder modules
--------------

- c/c_prot
    Collection of gclasses working with protocols.

- js/w2ui-artgins
    javascript framework used in GUI apps

Folder performance
------------------

- c/perf_yev_ping_pong
    Benchmarks
- c/perf_yev_timer

Folder scripts
--------------

Python scripts used in TUI

Folder tests
------------

Tests.

C test is using Criterion.

Folder tools
------------

- cmake
    Auxiliary tools: cmake files,...
- web-skeleton3
    Tool for make static web apps

Folder yunos
------------

Full applications or utilities built with yunetas

- gui_yunetas.js
    GUI of Yunetas.
- tui_yunetas.py
    TUI of Yunetas.
