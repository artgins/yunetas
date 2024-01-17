External libraries
==================

Libraries used by Yunetas:

    - `Criterion <https://github.com/Snaipe/Criterion>`_
    - `jansson <https://github.com/akheron/jansson>`_
    - `libjwt <https://github.com/benmcollins/libjwt>`_
    - `liburing <https://github.com/axboe/liburing>`_
    - `openssl <https://github.com/openssl/openssl>`_
    - `mbedtls <https://github.com/Mbed-TLS/mbedtls>`_
    - `pcre2 <https://github.com/PCRE2Project/pcre2>`_
    - `http-parser <https://github.com/nodejs/http-parser>`_
    - `linenoise <https://github.com/antirez/linenoise>`_

Utilities used by Yunetas:

    - `openresty <https://github.com/openresty/openresty>`_
    - `nginx <https://github.com/nginx>`_

The external libraries will be integrated in the yuneta kernel as static libraries.

To avoid conflicts with other versions of libuv and jannson installed in your host,
the libraries will be deployed in ``/yuneta/development/outputs/lib``
and the include files in ``/yuneta/development/outputs/include``.

Build with the next scripts::

    * extrae.sh
    * configure-libs.sh
    * install-libs.sh
