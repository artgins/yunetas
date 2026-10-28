#!/bin/bash

source ./repos2clone.sh

#  Exit immediately if a command exits with a non-zero status.
set -e

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   liburing
#------------------------------------------
echo "===================== liburing ======================="
cd build/liburing
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build/mbedtls
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   openssl
#------------------------------------------
echo "===================== OPENSSL ======================="
cd build/openssl
make install
cd ../..

#------------------------------------------
#   PCRE
#------------------------------------------
echo "===================== PCRE2 ======================="
cd build/pcre2/build
make install
cd ..
cd ../..

#------------------------------------------
#   libbacktrace
#------------------------------------------
echo "===================== libbacktrace ======================="
cd build/libbacktrace
make install
cd ../..

#------------------------------------------
#   argp-standalone
#------------------------------------------
echo "===================== ARGP-STANDALONE ======================="
cd build/argp-standalone
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   libncurses
#------------------------------------------
echo "===================== NCURSES ======================="
cd build/ncurses

make install
cd ../..


#------------------------------------------
#   nginx
#------------------------------------------
echo "===================== NGINX ======================="
cd build/nginx

make install
cd ../..


#------------------------------------------
#   openresty
#------------------------------------------
#   configure-libs.sh does not build it on a target its LuaJIT does not
#   support (riscv64), and asks the same question.
#
echo "===================== OPENRESTY ======================="
LUAJIT_SRC=$(ls -d build/openresty/openresty-"$TAG_OPENRESTY"/bundle/LuaJIT-*/src 2>/dev/null | head -n1)
if [ -z "$LUAJIT_SRC" ]; then
    echo "ERROR: no LuaJIT in build/openresty/openresty-$TAG_OPENRESTY/bundle: run ./extrae.sh" >&2
    exit 1
fi

if ${CC:-cc} -E -I"$LUAJIT_SRC" "$LUAJIT_SRC/lj_arch.h" >/dev/null 2>&1; then
    cd build/openresty
    cd "openresty-$TAG_OPENRESTY"

    make install
    cd ..
    cd ../..
else
    echo "WARNING: openresty NOT installed: its LuaJIT does not support $(${CC:-cc} -dumpmachine)." >&2
fi
