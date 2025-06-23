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
echo "===================== PCRE ======================="
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
#   libjwt
#------------------------------------------
echo "===================== LIBJWT ======================= $TAG_LIBJWT"
cd build/libjwt
cd build
make install
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
