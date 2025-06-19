#!/bin/bash

#
#   version 1.1
#       upgrade to liburing-2.9
#   version 1.2
#       upgrade to liburing-2.11
#

VERSION="1.2-s"

source ./repos2clone-static.sh
CFLAGS="-Wno-error=char-subscripts -O3 -g -ggdb -fPIC"
#CFLAGS+=" -I/lib/modules/$(uname -r)/build/include"
export CFLAGS
export PKG_CONFIG_PATH=/yuneta/development/outputs_ext/lib/pkgconfig
export CC=musl-gcc
export LDFLAGS="-static"

[ -f "./VERSION_INSTALLED.txt" ] && rm "./VERSION_INSTALLED.txt"

#  Exit immediately if a command exits with a non-zero status.
set -e


#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#
#   YUNETA_INSTALL_PREFIX by default:
#       --prefix=/yuneta/development/outputs_ext
#-----------------------------------------------------#
if [ -n "$YUNETAS_BASE" ]; then
    YUNETAS_BASE_DIR="$YUNETAS_BASE"
else
    YUNETAS_BASE_DIR="/yuneta/development/yunetas"
fi

PARENT_DIR=$(dirname "$YUNETAS_BASE_DIR")
YUNETA_INSTALL_PREFIX="${PARENT_DIR}/outputs_ext"
mkdir -p "$YUNETA_INSTALL_PREFIX"

#------------------------------------------
#   Jansson OK
#------------------------------------------
#echo "===================== JANSSON ======================="
#cd build/jansson
#mkdir -p build
#cd build
#
#git checkout "$TAG_JANSSON"
#
#cmake .. \
#    -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
#    -DJANSSON_BUILD_DOCS=OFF \
#    -DJANSSON_BUILD_SHARED_LIBS=OFF
#
#make
#make install
#cd ..
#cd ../..

#------------------------------------------
#   mbedtls OK
#------------------------------------------
#echo "===================== MBEDTLS ======================="
#cd build/mbedtls
#mkdir -p build
#cd build
#
#git checkout "$TAG_MBEDTLS"
#
#cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
#  -DENABLE_TESTING=Off -DCMAKE_BUILD_TYPE=Debug ..
#make
#make install
#cd ..
#cd ../..

#------------------------------------------
#   PCRE OK
#------------------------------------------
#echo "===================== PCRE2 ======================="
#cd build/pcre2
#
#git checkout "$TAG_PCRE2"
#git submodule update --init
#
#mkdir -p build
#cd build
#cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
#    -DBUILD_STATIC_LIBS=ON \
#    -DBUILD_SHARED_LIBS=OFF \
#    -DPCRE2_BUILD_PCRE2_16=ON \
#    -DPCRE2_BUILD_PCRE2_32=ON \
#    -DPCRE2_STATIC_PIC=ON \
#    -DPCRE2_SUPPORT_JIT=ON \
#    ..
#
#make
#make install
#cd ..
#cd ../..

#------------------------------------------
#   libbacktrace OK
#------------------------------------------
#echo "===================== libbacktrace ======================="
#cd build/libbacktrace
#
#./configure --prefix="${YUNETA_INSTALL_PREFIX}"
#make
#make install
#cd ../..

#------------------------------------------
#   liburing OK
#------------------------------------------
#echo "===================== liburing ======================="
#cd build/liburing
#
#git checkout "$TAG_LIBURING"
#
#./configure \
#    --prefix="${YUNETA_INSTALL_PREFIX}" \
#    --cc=clang
#
#make
#make install
#cd ../..

#------------------------------------------
#   argp-standalone
#------------------------------------------
#echo "===================== ARGP-STANDALONE ======================="
#cd build/argp-standalone
#mkdir -p build
#cd build
#
#git checkout "$TAG_ARGP_STANDALONE"
#
#cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}"  ..
#make
#make install
#cd ..
#cd ../..

#------------------------------------------
#   libjwt ERROR
#------------------------------------------
echo "===================== LIBJWT ======================="
cd build/libjwt
mkdir -p build
cd build

CFLAGS="-Wno-error=char-subscripts -O3 -g -ggdb -fPIC" # TODO valid for all?
export CFLAGS
export PKG_CONFIG_PATH=/yuneta/development/outputs_ext/lib/pkgconfig
export CC=musl-gcc
export LDFLAGS="-static"

git checkout "$TAG_LIBJWT"

cmake \
    -DCMAKE_INSTALL_PREFIX:PATH="/yuneta/development/outputs_ext" \
    -DEXCLUDE_DEPRECATED=TRUE \
    -DWITH_GNUTLS=OFF \
    -DWITH_MBEDTLS=ON \
    ..

make
make install

cd ..
cd ../..


#------------------------------------------
#   Save the version installed
#------------------------------------------
echo "$VERSION" > VERSION_INSTALLED.txt
echo "" >> VERSION_INSTALLED.txt
