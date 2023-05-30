Yunetas
=======

Yuneta Simplified, a complete asynchronous C development framework.

Inherited from `ginsfms <https://pypi.org/project/ginsfsm/>`_ and `Yuneta <http://yuneta.io>`_.

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
        - esp_gc_prot       (depends of esp_yuneta)

    It's only necessary to include the component with higher dependency

- core-linux:
    Core for linux
- gc_prot
    Collection of gclasses working with protocols
- yunos:
    Full applications or utilities built with yunetas.
