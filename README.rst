Yunetas
=======

Yuneta Simplified, a complete asynchronous C development framework for Linux and ESP32.

Inherited from `ginsfsm <https://pypi.org/project/ginsfsm/>`_ and `Yuneta <http://yuneta.io>`_.

Directories
-----------

- gobj:
    G-Objects, implementation of classes and objects based in a simple Finite State Machine,
    attributes based in json, a wide table of class methods,
    and an api to let communicate between objects through events,
    with publish/subscribe pattern integrated.

- core-esp32:
    Core for esp32 microcontroller (`esp-idf <https://docs.espressif.com/projects/esp-idf/>`_).
    List of components order by dependency (bottom higher dependency) ::
        - esp_jansson
        - esp_gobj          (depends of esp_jansson)
        - esp_yuneta        (depends of esp_gobj)
        - esp_c_prot        (depends of esp_yuneta)

    It's only necessary to include the component with higher dependency

- core-linux:
    Core for linux, based in `io_uring <https://github.com/axboe/liburing>`_.

- c_prot
    Collection of gclasses working with protocols.

- external-libs:
    Dependencies of Yunetas

- yunos:
    Full applications or utilities built with yunetas (TODO).

- tools:
    Auxiliary tools: cmake files,...

- tests:
    Tests built with Criterion

- performance:
    Benchmarks


Build
-----

To build with tests::

   mkdir build && cd build
   cmake ..
   cmake --build .  --target install
   ctest    # only if you want to test


To build without tests::

   mkdir build && cd build
   cmake -D ENABLE_TESTS=OFF ..
   cmake --build .  --target install
