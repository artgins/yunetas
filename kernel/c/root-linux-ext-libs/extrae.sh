#!/bin/bash

sudo apt -y install libjansson-dev          # required for libjwt
sudo apt -y install libpcre2-dev            # required by openresty
sudo apt -y install perl dos2unix mercurial # required by openresty

if ! command -v ldconfig >/dev/null 2>&1; then
    echo "ldconfig is not available in PATH. Exiting. Add /usr/sbin/ to PATH"
    exit 1
fi

if ! command -v meson >/dev/null 2>&1; then
    echo "meson is not available in PATH. Exiting. Run 'sudo apt install meson'"
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "ninja is not available in PATH. Exiting. Run 'sudo apt install ninja-build'"
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
git clone https://github.com/Snaipe/Criterion.git

echo "extrae libjwt"
git clone https://github.com/benmcollins/libjwt.git

echo "extrae openresty"
git clone https://github.com/openresty/openresty.git


cd ..
