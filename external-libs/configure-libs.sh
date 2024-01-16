#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

export CFLAGS="-Wno-error=char-subscripts -O0 -g3 -ggdb"

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson-artgins
mkdir -p build
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
cd build/liburing-liburing-2.5
./configure --prefix=/yuneta/development/outputs
make
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build/mbedtls-3.5.1
mkdir -p build
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
cd build/openssl-3.2.0
./config \
    --prefix=/yuneta/development/outputs \
    --openssldir=/yuneta/bin/ssl3 \
    --libdir=lib \
    no-tests \
    no-shared \
    no-docs \
    enable-ssl-trace
make
make install
cd ../..


#------------------------------------------
#   criterion
#------------------------------------------
echo "===================== Criterion ======================="
cd build/Criterion-2.4.2
meson setup \
    --prefix=/yuneta/development/outputs \
    --libdir=/yuneta/development/outputs/lib \
    --default-library=static \
    build
ninja -C build install
cd ../..


#------------------------------------------
#   libjwt
#------------------------------------------
echo "===================== libjwt ======================="
cd build/libjwt-1.16.0
mkdir -p build
cd build
cmake -G "Ninja" \
    -DCMAKE_INSTALL_PREFIX:PATH=/yuneta/development/outputs \
    -DBUILD_EXAMPLES=OFF \
    ..

ninja
ninja install
cd ..
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
    --with-http_stub_status_module \
    --with-pcre-jit \
    --with-pcre=/yuneta/development/yunetas/external-libs/build/pcre2-10.42 \
    --with-openssl=/yuneta/development/yunetas/external-libs/build/openssl-3.2.0 \
    --with-openssl-opt=no-tests \
    --with-openssl-opt=no-shared \
    --with-openssl-opt=no-docs \
    --with-http_v2_module \
    --with-http_gzip_static_module \
    --add-module=/yuneta/development/yunetas/external-libs/build/ngx-http-auth-jwt-module-artgins \
    --with-ld-opt="/yuneta/development/outputs/lib/libjwt.a /yuneta/development/outputs/lib/libjansson.a"

make
make install
cd ../..


#------------------------------------------
#   PCRE
#------------------------------------------
echo "===================== PCRE ======================="
cd build/pcre2-10.42
./configure --prefix=/yuneta/development/outputs \
    --disable-shared \
    --enable-pcre2-16 \
    --enable-pcre2-32 \
    --enable-jit
make
make install
cd ../..
