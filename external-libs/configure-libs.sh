#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
# set -e

export CFLAGS="-Wno-error=char-subscripts -O0 -g3 -ggdb"

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson-2.14-gines
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH=/yuneta/development/outputs -DJANSSON_BUILD_DOCS=OFF ..
make
make install
cd ..

cd ../..


#------------------------------------------
#   Libunwind
#------------------------------------------
echo "===================== UNWIND ======================="
cd build/libunwind-1.7.0
autoreconf -i
./configure --prefix=/yuneta/development/outputs
make
make install
cd ../..


#------------------------------------------
#   Libuv
#------------------------------------------
echo "===================== LIBUV ======================="
cd build/libuv-1.45.0-gines
sh autogen.sh
./configure --prefix=/yuneta/development/outputs
make
make install
cd ../..

#------------------------------------------
#   libncurses
#------------------------------------------
echo "===================== NCURSES ======================="
cd build/ncurses-6.4

# HACK in recents gcc ncurses will fail.
# WARNING **Before** make configure_ncurses.sh do:
export CPPFLAGS="-P" #Fallo arreglado con la version ncurses-6.4 ?

./configure \
    --prefix=/yuneta/development/outputs \
    --datarootdir=/yunetas/bin/ncurses \
    --enable-sp-funcs
#         --enable-term-driver \
#         --enable-ext-putwin
make
make install
cd ../..

#------------------------------------------
#   PCRE
#------------------------------------------
# HACK WARNING en redhat usa ./configure
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
# HACK sudo yum install pcre-devel.x86_64 zlib-devel.x86_64
echo "===================== NGINX ======================="
cd build/nginx-1.24.0
./configure \
    --prefix=/yunetas/bin/nginx \
    --with-http_ssl_module \
    --with-stream \
    --with-stream_ssl_module
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
#   liburing WARNING  master version!
#------------------------------------------
echo "===================== liburing ======================="
cd build/liburing-master-2023-06-06
./configure --prefix=/yuneta/development/outputs
make
make install
cd ../..
