#!/bin/bash

source ./repos2clone.sh

#  Exit immediately if a command exits with a non-zero status.
set -e

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson-artgins
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   liburing WARNING  master version!
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
echo "===================== PCRE ======================="
cd build/pcre2/build
make install
cd ..
cd ../..

#------------------------------------------
#   libjwt
#------------------------------------------
cd build/libjwt
cd build
ninja install
cd ..
cd ../..

#------------------------------------------
#   openresty
#------------------------------------------
echo "===================== OPENRESTY ======================="
cd build/openresty
cd "openresty-$TAG_OPENRESTY"

make install
cd ..
cd ../..

# Fix these old dependencies, the new cause errors. NEWS: it seems that works with last version
#/yuneta/bin/openresty/bin/opm --install-dir=/yuneta/bin/openresty install zmartzone/lua-resty-openidc=1.7.5
#/yuneta/bin/openresty/bin/opm --install-dir=/yuneta/bin/openresty install bungle/lua-resty-session=4.0.5
