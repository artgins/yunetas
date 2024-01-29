Yunetas
=======

Yuneta Simplified is a complete asynchronous development framework.

For Linux and RTOS/ESP32.

It can be transport to any language.

Versions in C, Javascript, [soon Python].


Directories
===========

Top folders:

    - kernel

    - libs

    - performance

    - tests

    - tools

    - yunos

Folder kernel
-------------

- gobj-c:
    G-Objects, implementation of classes and objects based in a simple Finite State Machine,
    attributes based in json, a wide table of class methods,
    and an api to let communicate between objects through events,
    with publish/subscribe pattern integrated.

- root-esp32-c:
    Core for esp32 microcontroller (`esp-idf <https://docs.espressif.com/projects/esp-idf/>`_).
    List of components order by dependency (bottom higher dependency) ::
        - esp_jansson
        - esp_gobj          (depends of esp_jansson)
        - esp_yuneta        (depends of esp_gobj)
        - esp_c_prot        (depends of esp_yuneta)

    It's only necessary to include the component with higher dependency

- root-linux-c:
    Core for linux, based in `io_uring <https://github.com/axboe/liburing>`_.

- root-linux-c-ext-libs:
    External libraries used by root-linux-c, static and self compiled.


Folder libs
-----------

- c_prot
    Collection of gclasses working with protocols.

Folder performance
------------------

Benchmarks

Folder tests
------------

Tests.

C test is using Criterion.

Folder yunos
------------

Full applications or utilities built with yunetas (TODO).

Folder tools
------------

Auxiliary tools: cmake files,...


Folder build
------------

To build and install, with tests::

   mkdir build && cd build
   cmake ..
   cmake --build . --target install    # OR make && make install
   ctest    # to run tests


To build without tests::

   mkdir build && cd build
   cmake -D ENABLE_TESTS=OFF ..
   cmake --build . --target install

By default the installation directory of include files,
libraries and binaries is ``/yuneta/development/outputs/``

