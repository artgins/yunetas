External libraries
==================

The `gobj-ecosistema <https://bitbucket.org/account/user/yuneta/projects/GOBJ>`_ are based
in the great C libraries:

    - `libuv <http://libuv.org/>`_
    - `jansson <http://www.digip.org/jansson/>`_
    - `linenoise <https://github.com/antirez/linenoise>`_
    - `http-parser <http://github.com/joyent/http-parser>`_
    - `pcre2 <www.pcre.org/>`_
    - `unwind <http://www.nongnu.org/libunwind/>`_
    - ...

The external libraries will be integrated in the yuneta kernel as static libraries.

To avoid conflicts with other versions of libuv and jannson installed in your host,
the libraries will be deployed in ``/yuneta/development/outputs/lib``
and the include files in ``/yuneta/development/outputs/include``.

Build with the next scripts::

    * extrae.sh
    * configure-libs.sh
    * install-libs.sh

