#!/bin/bash

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
#   criterion
#------------------------------------------
echo "===================== Criterion ======================="
cd build/Criterion
ninja -C build install
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
export TAG_OPENRESTY="1.25.3.1" # WARNING repeated in configure-libs.sh
cd build/openresty
cd "openresty-$TAG_OPENRESTY"

make install
cd ..
cd ../..

# Fix these old dependencies, the new cause errors
/yuneta/bin/openresty/bin/opm --install-dir=/yuneta/bin/openresty install zmartzone/lua-resty-openidc=1.7.5
/yuneta/bin/openresty/bin/opm --install-dir=/yuneta/bin/openresty install bungle/lua-resty-session=3.10
