Yunetas
=======

Yuneta Simplified is a complete asynchronous development framework.

For Linux and RTOS/ESP32.

It can be transport to any language.

Versions in C, Javascript, [soon Python].

Installation
============

Install system dependencies
---------------------------

Use `apt` to install the required dependencies::

    sudo apt install --no-install-recommends \
      git mercurial make cmake ninja-build \
      gcc musl musl-dev musl-tools clang \
      python3-dev python3-pip python3-setuptools python3-tk python3-wheel python3-venv \
      libjansson-dev libpcre2-dev perl dos2unix

Install pipx
------------

[pipx] is used to install Python CLI applications globally while still isolating them in virtual environments.
[pipx] will manage upgrades and uninstalls when used to install Poetry.


Create a new virtual environment ::

    python3 -m venv ~/yunetaproject/.yuneta

Activate the virtual environment: ::

    source ~/yunetaproject/.yuneta/bin/activate


Clone
-----

Clone with submodules::

    cd ~/yunetaproject

    git clone --recurse-submodules https://github.com/artgins/yunetas.git

    cd ~/yunetaproject/yunetas
    source yunetas-env.sh

Install additional Python dependencies
--------------------------------------

Install additional Python dependencies::

    pip install -r ~/yunetaproject/yunetas/scripts/requirements.txt

Configure .bashrc
-----------------

Next times to activate yunetas environment,
(you can add these lines to ``~/.bashrc`` ::

    source ~/yunetaproject/.yuneta/bin/activate
    cd ~/yunetaproject/yunetas
    source yunetas-env.sh

Configuring (Kconfig)
---------------------

Configuration options are defined in ``Kconfig`` file.
The output from Kconfig is a header file ``yuneta_config.h`` with macros that can be tested at build time.

You can use any of this utilities to edit the Kconfig file:

     - kconfig-conf Kconfig
     - kconfig-mconf Kconfig
     - kconfig-nconf Kconfig
     - kconfig-qconf Kconfig

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


Basic usage
===========

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

[pipx]:   https://github.com/pypa/pipx
