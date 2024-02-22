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
git clone https://github.com/Mbed-TLS/mbedtls.git

echo "extrae openssl"
git clone https://github.com/openssl/openssl.git

echo "extrae pcre2"
git clone https://github.com/PCRE2Project/pcre2.git

echo "extrae criterion"
tar xzf ../sources/Criterion-2.4.2.tar.gz

echo "extrae libjwt"
tar xzf ../sources/libjwt-1.16.0.tar.gz

echo "extrae nginx"
tar xzf ../sources/nginx-1.25.3.tar.gz

echo "extrae openresty"
tar xzf ../sources/openresty-1.25.3.1.tar.gz


cd ..
