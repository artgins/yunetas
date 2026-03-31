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
#   version 1.5
#       upgrade to jansson v2.15.0
#       upgrade to liburing-2.14
#       upgrade to mbedtls v4.0.0
#       upgrade to openssl 3.6.1
#       upgrade to pcre2 2.10.47
#       upgrade to nginx 1.28.2
#   version 1.6
#       upgrade to openresty 1.29.2.3
#       upgrade to nginx 1.28.3
#
#   version 1.7
#       upgrade to mbedtls v4.1.0

VERSION="1.7"


source ./repos2clone.sh
export CFLAGS="-Wno-error=char-subscripts -O3 -g -DNDEBUG" # let each library to be, or not
export CC=cc

[ -f "./VERSION_INSTALLED.txt" ] && rm "./VERSION_INSTALLED.txt"

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

YUNETA_INSTALL_PREFIX="${YUNETAS_BASE}/outputs_ext"

rm -rf "$YUNETA_INSTALL_PREFIX"
mkdir -p "$YUNETA_INSTALL_PREFIX"

export PKG_CONFIG_PATH="$YUNETA_INSTALL_PREFIX/lib/pkgconfig"

#------------------------------------------
#   Jansson
#------------------------------------------
echo "===================== JANSSON ======================="
cd build/jansson

git checkout "$TAG_JANSSON"

mkdir -p build
cd build

cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" -DJANSSON_BUILD_DOCS=OFF ..
make
make install
cd ..
cd ../..

#------------------------------------------
#   liburing
#------------------------------------------
echo "===================== liburing ======================="
cd build/liburing

git checkout "$TAG_LIBURING"

./configure --prefix="${YUNETA_INSTALL_PREFIX}"

make
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
echo "===================== MBEDTLS ======================="
cd build/mbedtls

git checkout "$TAG_MBEDTLS"
git submodule update --init --recursive

VENV_DIR=".venv"
# Create venv if it doesn't exist
if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
fi

# Activate venv
# shellcheck disable=SC1091
source "$VENV_DIR/bin/activate"

# Upgrade pip tooling inside the venv
python3 -m pip install --upgrade pip setuptools wheel

python3 -m pip install -r scripts/basic.requirements.txt

mkdir -p build
cd build

cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
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

git checkout "$TAG_OPENSSL"
git submodule update --init

#    --openssldir=/yuneta/bin/ssl3 \

./config \
    --prefix="${YUNETA_INSTALL_PREFIX}" \
    --libdir=lib \
    -fPIC \
    no-tests \
    no-shared \
    no-docs \
    no-dso \
    no-sock \
    enable-ssl-trace
make
make install
cd ../..

#------------------------------------------
#   PCRE
#------------------------------------------
echo "===================== PCRE2 ======================="
cd build/pcre2

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
    ..

make
make install
cd ..
cd ../..

#------------------------------------------
#   libbacktrace
#------------------------------------------
echo "===================== libbacktrace ======================="
cd build/libbacktrace

./configure --prefix="${YUNETA_INSTALL_PREFIX}"
make
make install
cd ../..

#------------------------------------------
#   argp-standalone
#------------------------------------------
echo "===================== ARGP-STANDALONE ======================="
cd build/argp-standalone

git checkout "$TAG_ARGP_STANDALONE"

mkdir -p build
cd build

cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}"  ..
make
make install
cd ..
cd ../..


#------------------------------------------
#   libncurses
#------------------------------------------
echo "===================== NCURSES ======================="
cd build/ncurses

git checkout "$TAG_NCURSES"

./configure \
    --prefix="${YUNETA_INSTALL_PREFIX}" \
    --datarootdir=/yuneta/bin/ncurses \
    --without-cxx --without-cxx-binding \
    --without-manpages \
    --enable-sp-funcs
make
make install
cd ../..


#------------------------------------------
#   nginx
#------------------------------------------
echo "===================== NGINX ======================="
cd build/nginx

git checkout "$TAG_NGINX"

./auto/configure \
    --prefix=/yuneta/bin/nginx \
    --with-http_ssl_module \
    --with-stream \
    --with-stream_ssl_module \
    --with-pcre=../pcre2 \
    --with-pcre-jit
make
make install
cd ../..


#------------------------------------------
#   Commands below can fail
#------------------------------------------
set +e

#------------------------------------------
#   openresty
#------------------------------------------
echo "===================== OPENRESTY ======================="
cd build/openresty

git checkout "v$TAG_OPENRESTY"
git submodule update --init

make
cd "openresty-$TAG_OPENRESTY"

./configure \
    --prefix=/yuneta/bin/openresty \
    --with-http_ssl_module \
    --with-http_realip_module \
    --with-stream \
    --with-stream_ssl_module \
    --with-http_stub_status_module \
    --with-pcre-jit \
    --with-http_v2_module \
    --with-http_gzip_static_module

gmake
gmake install
cd ..
cd ../..


#------------------------------------------
#   Save the version installed
#------------------------------------------
echo "Version $VERSION installed"
echo "$VERSION" > VERSION_INSTALLED.txt
echo "" >> VERSION_INSTALLED.txt

unset CC
unset CFLAGS
unset PKG_CONFIG_PATH
