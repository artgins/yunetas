#!/bin/bash

#
#   version 1.1
#       upgrade to liburing-2.9
#   version 1.2
#       upgrade to liburing-2.11
#   version 1.3
#       using musl-gcc
#

VERSION="1.3-s"

source ./repos2clone-static.sh

export CC=/usr/bin/musl-gcc
export CFLAGS="-Wno-error=char-subscripts -O3 -DNDEBUG" # let each library to be
export LDFLAGS="-static"

[ -f "./VERSION_INSTALLED_STATIC.txt" ] && rm "./VERSION_INSTALLED_STATIC.txt"

#  Exit immediately if a command exits with a non-zero status.
set -e

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#
#   YUNETA_INSTALL_PREFIX by default:
#       --prefix=/yuneta/development/outputs_ext_static
#-----------------------------------------------------#
if [ -n "$YUNETAS_BASE" ]; then
    YUNETAS_BASE_DIR="$YUNETAS_BASE"
else
    YUNETAS_BASE_DIR="/yuneta/development/yunetas"
fi

PARENT_DIR=$(dirname "$YUNETAS_BASE_DIR")
YUNETA_INSTALL_PREFIX="${PARENT_DIR}/outputs_ext_static"
mkdir -p "$YUNETA_INSTALL_PREFIX"

MUSL_TOOLCHAIN="$YUNETAS_BASE_DIR/tools/cmake/musl-toolchain.cmake"

export PKG_CONFIG_PATH="$YUNETA_INSTALL_PREFIX/lib/pkgconfig"

#------------------------------------------
#   Jansson OK
#------------------------------------------
echo "===================== JANSSON ======================="
cd build_static/jansson

git checkout "$TAG_JANSSON"

mkdir -p build
cd build

cmake .. \
    -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
    -DJANSSON_BUILD_DOCS=OFF \
    -DJANSSON_BUILD_SHARED_LIBS=OFF \
    -DJANSSON_EXAMPLES=OFF \
    -DJANSSON_WITHOUT_TESTS=ON \
    -DCMAKE_TOOLCHAIN_FILE="${MUSL_TOOLCHAIN}"

make
make install
cd ..
cd ../..

#------------------------------------------
#   liburing OK
#------------------------------------------
echo "===================== liburing ======================="
cd build_static/liburing

git checkout "$TAG_LIBURING"

./configure --prefix="${YUNETA_INSTALL_PREFIX}"

make
make install
cd ../..

#------------------------------------------
#   mbedtls OK
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build_static/mbedtls

git checkout "$TAG_MBEDTLS"
git submodule update --init

mkdir -p build
cd build

cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
    -DCMAKE_TOOLCHAIN_FILE="${MUSL_TOOLCHAIN}" \
    -DENABLE_TESTING=Off -DCMAKE_BUILD_TYPE=Debug ..
make
make install
cd ..
cd ../..

#------------------------------------------
#   openssl
#------------------------------------------
echo "===================== OPENSSL ======================="
cd build_static/openssl

git checkout "$TAG_OPENSSL"
git submodule update --init

./config \
    --prefix="${YUNETA_INSTALL_PREFIX}" \
    --openssldir=/yuneta/bin/ssl3 \
    --libdir=lib \
    -fPIC \
    no-tests \
    no-shared \
    no-docs \
    enable-ssl-trace
make
make install
cd ../..

#------------------------------------------
#   PCRE OK
#------------------------------------------
echo "===================== PCRE2 ======================="
cd build_static/pcre2

git checkout "$TAG_PCRE2"
git submodule update --init

mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DPCRE2_BUILD_PCRE2_16=ON \
    -DPCRE2_BUILD_PCRE2_32=ON \
    -DPCRE2_STATIC_PIC=ON \
    -DPCRE2_SUPPORT_JIT=ON \
    -DCMAKE_TOOLCHAIN_FILE="${MUSL_TOOLCHAIN}" \
    ..

make
make install
cd ..
cd ../..

#------------------------------------------
#   libbacktrace OK
#------------------------------------------
echo "===================== libbacktrace ======================="
cd build_static/libbacktrace

./configure --prefix="${YUNETA_INSTALL_PREFIX}"
make
make install
cd ../..

#------------------------------------------
#   argp-standalone
#------------------------------------------
echo "===================== ARGP-STANDALONE ======================="
cd build_static/argp-standalone

git checkout "$TAG_ARGP_STANDALONE"

mkdir -p build
cd build

cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
    -DCMAKE_TOOLCHAIN_FILE="${MUSL_TOOLCHAIN}" \
    ..
make
make install
cd ..
cd ../..

#------------------------------------------
#   Save the version installed
#------------------------------------------
echo "Version $VERSION installed"
echo "$VERSION" > VERSION_INSTALLED_STATIC.txt
echo "" >> VERSION_INSTALLED_STATIC.txt

unset CC
unset CFLAGS
unset LDFLAGS
unset PKG_CONFIG_PATH
