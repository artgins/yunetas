#!/bin/bash

#
#   version 1.1
#       upgrade to liburing-2.9
#   version 1.2
#       upgrade to liburing-2.11
#   version 1.3
#       using musl-gcc
#   version 1.4
#       upgrade to ncurses-6.4
#

VERSION="1.4-s"

source ./repos2clone-static.sh

export CC=/usr/bin/musl-gcc
export CFLAGS="-Wno-error=char-subscripts -O3 -g -DNDEBUG" # let each library to be, or not
export LDFLAGS="-static"

[ -f "./VERSION_INSTALLED_STATIC.txt" ] && rm "./VERSION_INSTALLED_STATIC.txt"

#  Exit immediately if a command exits with a non-zero status.
set -e

#-----------------------------------------------------#
#   Get yunetas base path:
#   Resolve YUNETAS_BASE:
#       1) $YUNETAS_BASE if valid dir,
#       2) /yuneta/development/yunetas,
#       3) /yuneta/development,
#       else fail.
#-----------------------------------------------------#
# If env is set but invalid, warn and ignore
if [[ -n "${YUNETAS_BASE:-}" && ! -d "$YUNETAS_BASE" ]]; then
    echo "Warning: YUNETAS_BASE is set to '$YUNETAS_BASE' but is not a directory. Falling back..." >&2
    unset YUNETAS_BASE
fi

# Pick first existing candidate
if [[ -z "${YUNETAS_BASE:-}" ]]; then
    for d in "/yuneta/development/yunetas" "/yuneta/development"; do
        if [[ -d "$d" ]]; then
            YUNETAS_BASE="$d"
            break
        fi
    done
fi

# Hard fail if still unset
if [[ -z "${YUNETAS_BASE:-}" ]]; then
    echo "Error: Could not determine YUNETAS_BASE. Set the env var or ensure /yuneta/development[/yunetas] exists." >&2
    exit 1
fi

export YUNETAS_BASE
echo "Using YUNETAS_BASE: $YUNETAS_BASE"

# Optional: verify a required file (uncomment if needed)
# req="${YUNETAS_BASE}/tools/cmake/project.cmake"
# if [[ ! -f "$req" ]]; then
#     echo "Error: Missing required file: $req" >&2
#     exit 1
# fi

YUNETA_INSTALL_PREFIX="${YUNETAS_BASE}/outputs_ext_static"

rm -rf "$YUNETA_INSTALL_PREFIX"
mkdir -p "$YUNETA_INSTALL_PREFIX"

MUSL_TOOLCHAIN="$YUNETAS_BASE/tools/cmake/musl-toolchain.cmake"

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
