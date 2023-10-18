#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson-gines-2.14
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   liburing WARNING  master version!
#------------------------------------------
echo "===================== liburing ======================="
cd build/liburing-liburing-2.4
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build/mbedtls-3.4.0
cd build
make install
cd ..
cd ../..


#------------------------------------------
#   openssl
#------------------------------------------
#echo "===================== OPENSSL ======================="
#cd build/openssl-3.1.3
#make install
#cd ../..


#------------------------------------------
#   PCRE
#------------------------------------------
# HACK WARNING en redhat usa ./configure
echo "===================== PCRE ======================="
cd build/pcre2-10.42
make install
cd ../..


#------------------------------------------
#   nginx
#------------------------------------------
# HACK sudo yum install pcre-devel.x86_64 zlib-devel.x86_64
echo "===================== NGINX ======================="
cd build/nginx-1.24.0
make install
cd ../..

#------------------------------------------
#   criterion
#------------------------------------------
echo "===================== Criterion ======================="
cd build/Criterion-2.4.2
ninja -C build install
cd ../..
