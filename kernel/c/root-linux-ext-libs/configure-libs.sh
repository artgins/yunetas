#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

export CFLAGS="-Wno-error=char-subscripts -O0 -g3 -ggdb"

#------------------------------------------
#   VERSIONS
#------------------------------------------

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson-artgins
mkdir -p build
cd build

TAG_JANSSON="2.14a"
git checkout "$TAG_JANSSON"

cmake -DCMAKE_INSTALL_PREFIX:PATH=/yuneta/development/outputs -DJANSSON_BUILD_DOCS=OFF ..
make
make install
cd ..
cd ../..

#------------------------------------------
#   liburing
#------------------------------------------
echo "===================== liburing ======================="
cd build/liburing

TAG_LIBURING="liburing-2.5"
git checkout "$TAG_LIBURING"

./configure --prefix=/yuneta/development/outputs
make
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build/mbedtls
mkdir -p build
cd build

TAG_MBEDTLS="v3.5.2"
git checkout "$TAG_MBEDTLS"

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
cd build/openssl

TAG_OPENSSL="openssl-3.2.1"
git checkout "$TAG_OPENSSL"

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
#   PCRE
#------------------------------------------
echo "===================== PCRE2 ======================="
cd build/pcre2

TAG_PCRE2="pcre2-10.43"
git checkout "$TAG_PCRE2"

mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/yuneta/development/outputs \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DPCRE2_BUILD_PCRE2_16=ON \
    -DPCRE2_BUILD_PCRE2_32=ON \
    -DPCRE2_STATIC_PIC=ON \
    -DPCRE2_SUPPORT_JIT=ON \
    ..

make
make install
cd ..
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
cd build/nginx-1.25.3
./configure \
    --prefix=/yuneta/bin/nginx \
    --with-http_ssl_module \
    --with-stream \
    --with-stream_ssl_module \
    --with-http_stub_status_module \
    --with-pcre-jit \
    --with-openssl=../openssl-3.2.1 \
    --with-openssl-opt=no-tests \
    --with-openssl-opt=no-shared \
    --with-openssl-opt=no-docs \
    --with-http_v2_module \
    --with-http_gzip_static_module

make
make install
cd ../..


#------------------------------------------
#   openresty
#------------------------------------------
echo "===================== OPENRESTY ======================="
cd build/openresty-1.25.3.1
./configure \
    --prefix=/yuneta/bin/openresty \
    --with-http_ssl_module \
    --with-stream \
    --with-stream_ssl_module \
    --with-http_stub_status_module \
    --with-pcre-jit \
    --with-openssl=../openssl-3.2.1 \
    --with-openssl-opt=no-tests \
    --with-openssl-opt=no-shared \
    --with-openssl-opt=no-docs \
    --with-http_v2_module \
    --with-http_gzip_static_module

gmake
gmake install
cd ../..

/yuneta/bin/openresty/bin/opm --install-dir=/yuneta/bin/openresty install zmartzone/lua-resty-openidc=1.7.5
/yuneta/bin/openresty/bin/opm --install-dir=/yuneta/bin/openresty install bungle/lua-resty-session=3.10
