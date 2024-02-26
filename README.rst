Yunetas
=======

Yuneta Simplified is a complete asynchronous development framework.

For Linux and RTOS/ESP32.

It can be transport to any language.

Versions in C, Javascript, [soon Python].

Getting Started
===============

Install dependencies
--------------------

Use ``apt`` to install the required dependencies:

 .. code-block:: bash

    sudo apt install --no-install-recommends \
      git mercurial make cmake ninja-build \
      gcc musl musl-dev musl-tools clang \
      python3-dev python3-pip python3-setuptools python3-tk python3-wheel \
      libjansson-dev libpcre2-dev perl dos2unix \
      kconfig-frontends

Clone
-----

Clone with submodules::

    git clone --recurse-submodules https://github.com/artgins/yunetas.git

Get submodules if not got::

    git submodule update --init --recursive

    or?

    git submodule init
    git submodule update

Configuring (Kconfig)
---------------------

Configuration options are defined in ``Kconfig`` file.
The output from Kconfig is a header file ``yuneta_config.h`` with macros that can be tested at build time.

You can use any of this utilities to edit the Kconfig file:

     - ``kconfig-conf Kconfig``
     - ``kconfig-mconf Kconfig``
     - ``kconfig-nconf Kconfig``
     - ``kconfig-qconf Kconfig``

Compiling and Installing
------------------------

To build and install, with debug and tests::

    mkdir build && cd build
    cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
    ninja
    ninja install
    ctest    # to run tests


To build without debug::

    mkdir build && cd build
    cmake -GNinja ..
    ninja
    ninja install
    ctest    # to run tests

By default the installation directory of include files,
libraries and binaries is ``/yuneta/development/outputs/``


Directories
===========

Top folders:
    - doc
    - kernel
    - libs
    - performance
    - tests
    - tools
    - yunos

Folder doc
----------

YunetaS Documentation implemented with sphinx.

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

Folder tools
------------

Auxiliary tools: cmake files,...

Folder yunos
------------

Full applications or utilities built with yunetas (TODO).
