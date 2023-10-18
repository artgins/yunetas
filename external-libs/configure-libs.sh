#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

export CFLAGS="-Wno-error=char-subscripts -O0 -g3 -ggdb"

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson-gines-2.14
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/yuneta/development/outputs -DJANSSON_BUILD_DOCS=OFF ..
make
make install
cd ..
cd ../..

#------------------------------------------
#   liburing
#------------------------------------------
echo "===================== liburing ======================="
cd build/liburing-liburing-2.4
./configure --prefix=/yuneta/development/outputs
make
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build/mbedtls-3.4.0
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/yuneta/development/outputs \
  -DENABLE_TESTING=Off -DCMAKE_BUILD_TYPE=Debug ..
make
make install
cd ..
cd ../..


#------------------------------------------
#   openssl
#------------------------------------------
echo "===================== OPENSSL ======================="
cd build/openssl-3.1.3
./config \
    --prefix=/yuneta/development/outputs \
    --openssldir=/yuneta/bin/ssl3 \
    --libdir=lib \
    no-tests \
    enable-ssl-trace
make
make install
cd ../..


#------------------------------------------
#   PCRE
#------------------------------------------
echo "===================== PCRE ======================="
cd build/pcre2-10.42
./configure --prefix=/yuneta/development/outputs \
    --enable-jit
make
make install
cd ../..


#------------------------------------------
#   nginx
#------------------------------------------
echo "===================== NGINX ======================="
cd build/nginx-1.24.0
./configure \
    --prefix=/yuneta/bin/nginx \
    --with-http_ssl_module \
    --with-stream \
    --with-stream_ssl_module \
    --with-pcre=/yuneta/development/yuneta/yunetas/external-libs/build/pcre2-10.42 \
    --with-pcre-jit \
    --with-openssl=/yuneta/development/yuneta/yunetas/external-libs/build/openssl-3.1.3 \
    --with-openssl-opt=no-tests
make
make install
cd ../..

#------------------------------------------
#   criterion
#------------------------------------------
echo "===================== Criterion ======================="
cd build/Criterion-2.4.2
meson setup --prefix=/yuneta/development/outputs --libdir=/yuneta/development/outputs/lib build
ninja -C build install
cd ../..


#------------------------------------------
#   libjwt
#------------------------------------------
#echo "===================== libjwt ======================="
#cd build/libjwt-1.16.0
#mkdir build
#cd build
#cmake -G "Ninja" -DCMAKE_INSTALL_PREFIX:PATH=/yuneta/development/outputs \
#      -DUSE_INSTALLED_JANSSON=OFF \
#      -DJANSSON_BUILD_DOCS=OFF \
#      ..
#
#ninja
#ninja install
#cd ..
#cd ../..
