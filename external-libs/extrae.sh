#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

rm -rf build/
mkdir build
cd build

echo "extrae jannson"
tar xzf ../sources/jansson-2.14-gines.tar.gz

echo "extrae libunwind"
tar xzf ../sources/libunwind-1.7.0.tar.gz

echo "extrae libuv"
tar xzf ../sources/libuv-1.45.0-gines.tar.gz

echo "extrae ncurses"
tar xzf ../sources/ncurses-6.4.tar.gz

echo "extrae pcre2"
tar xzf ../sources/pcre2-10.42.tar.gz

echo "extrae nginx"
tar xzf ../sources/nginx-1.24.0.tar.gz

echo "extrae mbedtls"
tar xzf ../sources/mbedtls-3.4.0.tar.gz

echo "extrae liburing"
tar xzf ../sources/liburing-master-2023-06-06.tar.gz


cd ..
