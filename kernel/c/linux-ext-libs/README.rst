External libraries
==================

Libraries used by Yunetas:

    - `jansson-artgins <https://github.com/artgins/jansson-artgins>`_
    - `liburing <https://github.com/axboe/liburing>`_
    - `mbedtls <https://github.com/Mbed-TLS/mbedtls>`_
    - `openssl <https://github.com/openssl/openssl>`_
    - `pcre2 <https://github.com/PCRE2Project/pcre2>`_
    - `libjwt <https://github.com/benmcollins/libjwt>`_
    - `http-parser <https://github.com/nodejs/http-parser>`_
    - `linenoise <https://github.com/antirez/linenoise>`_

Utilities used by Yunetas:

    - `openresty <https://github.com/openresty/openresty>`_

If you change some version of those libraries rembember to change the VERSION of installation in configure-libs.sh

The external libraries will be integrated in the yuneta kernel as static libraries.

To avoid conflicts with other versions of libuv and jansson installed in your host,
the libraries will be deployed in ``/yuneta/development/outputs/lib``
and the include files in ``/yuneta/development/outputs/include``.

Build with the next scripts::

    * extrae.sh
    * configure-libs.sh
    * install-libs.sh

# TODO WARNING Functions needed to avoid shared glibc:
#   getgrgid()
#   getgrnam()
#   getgrouplist()
#   getaddrinfo()
#   getpwuid()
#   gethostbyname() // from libcrypto: in function `BIO_gethostbyname
