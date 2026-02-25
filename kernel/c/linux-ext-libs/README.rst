External libraries
==================

Libraries used by Yunetas:

    - `jansson-artgins <https://github.com/akheron/jansson.git>`_
    - `liburing <https://github.com/axboe/liburing>`_
    - `mbedtls <https://github.com/Mbed-TLS/mbedtls>`_
    - `openssl <https://github.com/openssl/openssl>`_
    - `pcre2 <https://github.com/PCRE2Project/pcre2>`_
    - `libbacktrace <https://github.com/ianlancetaylor/libbacktrace>`_
    - `argp-standalone <https://github.com/artgins/argp-standalone.git>`_
    - `ncurses <https://github.com/mirror/ncurses.git>`_
    - `http-parser <https://github.com/nodejs/http-parser>`_
    - `linenoise <https://github.com/antirez/linenoise>`_

Utilities used by Yunetas:

    - `nginx <https://github.com/nginx/nginx.git>`_
    - `openresty <https://github.com/openresty/openresty.git>`_

If you change some version of those libraries remember to change the VERSION of installation in configure-libs.sh

The external libraries will be integrated in the yuneta kernel as static libraries.

To avoid conflicts with other versions of those libraries installed in the machine,
the libraries are cloned, compiled and deployed in own yuneta's directories and linked as static libraries.

If you want the libraries be compiled with the same compiler as yuneta, firstly execute `menuconfig`, select the compiler, and execute the script `set_compiler.sh`.
Build with the next scripts::

    * extrae.sh             # clone libraries
    * configure-libs.sh     # build and install libraries
    * re-install-libs.sh    # only to re-install libraries

TODO WARNING Functions needed to avoid shared glibc:
----------------------------------------------------

   - getgrgid()
   - getgrnam()
   - getgrouplist()
   - getaddrinfo()
   - getpwuid()
   - gethostbyname() // from libcrypto: in function `BIO_gethostbyname
