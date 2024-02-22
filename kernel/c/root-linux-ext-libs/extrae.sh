#!/bin/bash

sudo apt -y install libpcre2-dev

if ! command -v ldconfig >/dev/null 2>&1; then
    echo "ldconfig is not available in PATH. Exiting. Add /usr/sbin/ to PATH"
    exit 1
fi


#  Exit immediately if a command exits with a non-zero status.
set -e

rm -rf build/
mkdir build
cd build

echo "getting jansson-artgins"
git clone https://github.com/artgins/jansson-artgins.git

echo "getting liburing"
git clone https://github.com/axboe/liburing.git

echo "extrae mbedtls"
tar xzf ../sources/mbedtls-3.5.2.tar.gz

echo "extrae openssl"
tar xzf ../sources/openssl-3.2.1.tar.gz

echo "extrae pcre2"
tar xzf ../sources/pcre2-10.42.tar.gz

echo "extrae criterion"
tar xzf ../sources/Criterion-2.4.2.tar.gz

echo "extrae libjwt"
tar xzf ../sources/libjwt-1.16.0.tar.gz

echo "extrae nginx"
tar xzf ../sources/nginx-1.25.3.tar.gz

echo "extrae openresty"
tar xzf ../sources/openresty-1.25.3.1.tar.gz


cd ..
