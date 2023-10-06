#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

rm -rf build/
mkdir build
cd build

echo "extrae jansson"
tar xzf ../sources/jansson-gines-2.14.tar.gz

echo "extrae liburing"
tar xzf ../sources/liburing-liburing-2.4.tar.gz

echo "extrae mbedtls"
tar xzf ../sources/mbedtls-3.4.0.tar.gz

echo "extrae openssl"
tar xzf ../sources/openssl-3.1.3.tar.gz

echo "extrae pcre2"
tar xzf ../sources/pcre2-10.42.tar.gz

echo "extrae nginx"
tar xzf ../sources/nginx-1.24.0.tar.gz

echo "extrae nginx"
tar xzf ../sources/nginx-1.24.0.tar.gz

echo "extrae libjwt"
tar xzf ../sources/libjwt-1.16.0.tar.gz

echo "extrae ngx-http-auth-jwt-module"
tar xzf ../sources/ngx-http-auth-jwt-module-2.0.2.tar.gz

cd ..
