#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson-artgins-2.14a
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   liburing WARNING  master version!
#------------------------------------------
echo "===================== liburing ======================="
cd build/liburing-liburing-2.5
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build/mbedtls-3.5.1
cd build
make install
cd ..
cd ../..


#------------------------------------------
#   openssl
#------------------------------------------
echo "===================== OPENSSL ======================="
cd build/openssl-3.2.0
make install
cd ../..


#------------------------------------------
#   PCRE
#------------------------------------------
echo "===================== PCRE ======================="
cd build/pcre2-10.42
make install
cd ../..


#------------------------------------------
#   criterion
#------------------------------------------
echo "===================== Criterion ======================="
cd build/Criterion-2.4.2
ninja -C build install
cd ../..

#------------------------------------------
#   libjwt
#------------------------------------------
cd build/libjwt-1.16.0
cd build
ninja install
cd ..
cd ../..

#------------------------------------------
#   nginx
#------------------------------------------
echo "===================== NGINX ======================="
cd build/nginx-1.25.3
make install
cd ../..

#------------------------------------------
#   openresty
#------------------------------------------
echo "===================== OPENRESTY ======================="
cd build/openresty-1.25.3.1
make install
cd ../..

/yuneta/bin/openresty/bin/opm --install-dir=/yuneta/bin/openresty install zmartzone/lua-resty-openidc
