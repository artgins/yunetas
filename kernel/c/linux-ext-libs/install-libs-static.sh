#!/bin/bash

source ./repos2clone.sh

#  Exit immediately if a command exits with a non-zero status.
set -e

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build_static/jansson
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   liburing
#------------------------------------------
echo "===================== liburing ======================="
cd build_static/liburing
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build_static/mbedtls
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   PCRE
#------------------------------------------
echo "===================== PCRE ======================="
cd build_static/pcre2/build
make install
cd ..
cd ../..

#------------------------------------------
#   libbacktrace
#------------------------------------------
echo "===================== libbacktrace ======================="
cd build_static/libbacktrace
make install
cd ../..

#------------------------------------------
#   argp-standalone
#------------------------------------------
echo "===================== ARGP-STANDALONE ======================="
cd build_static/argp-standalone
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   libjwt
#------------------------------------------
cd build_static/libjwt
cd build
make install
cd ..
cd ../..
