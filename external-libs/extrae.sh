#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

rm -rf build/
mkdir build
cd build

echo "copy jansson"
cp -a ../jansson-artgins .

echo "extrae liburing"
tar xzf ../sources/liburing-liburing-2.5.tar.gz

echo "extrae mbedtls"
tar xzf ../sources/mbedtls-3.5.1.tar.gz

echo "extrae openssl"
tar xzf ../sources/openssl-3.2.0.tar.gz

echo "extrae pcre2"
tar xzf ../sources/pcre2-10.42.tar.gz

echo "extrae criterion"
tar xzf ../sources/Criterion-2.4.2.tar.gz

echo "extrae libjwt"
tar xzf ../sources/libjwt-1.16.0.tar.gz

echo "copy ngx-http-auth-jwt-module-artgins"
cp -a ../ngx-http-auth-jwt-module-artgins .

echo "extrae nginx"
tar xzf ../sources/nginx-1.24.0.tar.gz

cd ..
