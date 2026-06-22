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
#   version 1.8
#       add zlib v1.3.1
#       nginx and openresty now link statically against the builtin
#       OpenSSL, PCRE2 and zlib (build/) instead of the system libs
#       verify with ldd that no libssl/libcrypto/libpcre/libz comes
#       from the host
#       drop the legacy `set +e` before the openresty block
#   version 1.9
#       parallelise builds with MAKEFLAGS=-j$(nproc)
#       mbedtls: Debug -> Release, drop ENABLE_PROGRAMS, force PIC
#       jansson / pcre2 / argp-standalone: explicit Release + static + PIC
#       pcre2: drop pcre2grep and tests
#              (16-bit and 32-bit variants kept for downstream apps)
#       libbacktrace: --disable-shared --with-pic
#       ncurses: drop tests/progs/ada/debug, enable widec, force PIC
#       nginx: add http_v2 / realip / stub_status / gzip_static so it
#              matches openresty's module set
#       nginx + openresty: add --with-threads and --with-file-aio
#   version 1.10
#       revert the 1.8 "vendor everything into nginx/openresty" change.
#       The internal openssl + pcre2 builds are for yuneta only (yuneta
#       binaries link statically against outputs_ext/lib/*.a). nginx and
#       openresty are *separate* binaries shipped alongside yuneta, and
#       must link dynamically against the host's libssl / libcrypto /
#       libpcre / libz — same way every distro nginx package does.
#       Concretely:
#         - drop --with-openssl=../openssl + --with-openssl-opt=...
#         - drop --with-pcre=../pcre2 (was added for openresty in 1.8;
#           nginx had it earlier — both removed)
#         - drop --with-zlib=../zlib (already gone)
#         - drop the verify_static_linking block (its premise was that
#           libssl/libpcre/libz must NOT appear in ldd; that was wrong)
#       drop --enable-widec from ncurses. The 1.9 sweep enabled
#       wide-character ncurses, which installs as ncursesw (headers
#       in include/ncursesw/, lib as libncursesw.a). Yuneta's consumers
#       (modules/c/console, utils/c/{ycommand,ycli,mqtt_tui})
#       all include <ncurses/ncurses.h> and link ncurses.a (narrow API),
#       so the flag broke the whole console/CLI layer. Nobody uses the
#       wide API yet, so just drop the flag.
#   version 1.11
#       re-enable --enable-widec on ncurses, this time with the consumer
#       side also updated. Narrow ncurses without setlocale renders any
#       byte >= 0x80 through unctrl(), turning UTF-8 emoji into "M-x"
#       escapes in ycli/mqtt_tui (e.g. the "👤" prefix in BFF logs).
#       Wide ncurses installs libncursesw.a / libpanelw.a and headers
#       in include/ncursesw/. The 1.9 attempt failed because consumers
#       still linked panel.a/ncurses.a and included <ncurses/ncurses.h>.
#       Companion changes (must land together):
#         - modules/c/console/src/help_ncurses.{h,c}:
#               <ncurses/ncurses.h> -> <ncursesw/ncurses.h>
#               <ncurses/panel.h>   -> <ncursesw/panel.h>
#               setlocale(LC_ALL, "") before initscr()
#         - utils/c/ycommand/CMakeLists.txt,
#           utils/c/ycli/CMakeLists.txt,
#           utils/c/mqtt_tui/CMakeLists.txt:
#               panel.a -> panelw.a, ncurses.a -> ncursesw.a
#       Existing waddnstr / mvwaddstr calls keep working unchanged: the
#       narrow API in libncursesw is UTF-8-aware once the locale is set,
#       so it counts cells per character (not per byte).
#   version 1.12
#       upgrade to nginx release-1.30.1 (was release-1.28.3) to fix
#       CVE-2026-42945. nginx is a separate binary that links
#       dynamically against the host libssl/libpcre/libz (see version
#       1.10), so this is a pin-only bump: no yuneta consumer, header
#       or CMakeLists change rides along.
#       upgrade to openresty 1.29.2.4 (was 1.29.2.3): patch within the
#       same 1.29.2 series, same separate-dynamic-binary rationale.
#       upgrade to openssl 3.6.2 (was 3.6.1): patch within the 3.6
#       series, no API change. Stayed on 3.6 (not 4.0): 4.0 is non-LTS
#       (EOL 2027-05) and removes engines/legacy init; deferred as a
#       separate decision.
#   version 1.13
#       upgrade to nginx release-1.30.2 (was release-1.30.1) to fix
#       CVE-2026-9256 (buffer overflow in ngx_http_rewrite_module).
#       Same pin-only bump rationale as 1.12: nginx is a separate
#       binary linked dynamically against the host, no yuneta consumer
#       change rides along. openresty pin (1.29.2.4, based on nginx
#       1.29.2) is NOT covered by this fix — track upstream openresty
#       for a 1.29.2.5+ that picks up the patch before bumping it.
#   version 1.14
#       upgrade to openresty 1.29.2.5 (was 1.29.2.4): backports the
#       CVE-2026-9256 patch into the openresty-bundled nginx core
#       (released 2026-05-22, day after nginx 1.30.2). Also pulls in
#       a proxy_protocol v2 overflow/over-read hardening fix. Same
#       pin-only bump rationale.
#       (Compared to v1.13, this completes the CVE-2026-9256 coverage:
#        v1.13 patched the standalone nginx binary, v1.14 patches the
#        openresty binary that actually runs in front of yuneta SPAs.)

#   version 1.15
#       cross-distro install layout: force -DCMAKE_INSTALL_LIBDIR=lib on the
#       CMake-based libs (jansson, mbedtls, pcre2, argp-standalone). On RHEL
#       /Rocky/Alma, CMake's GNUInstallDirs defaults CMAKE_INSTALL_LIBDIR to
#       lib64, so mbedtls + pcre2 landed in outputs_ext/lib64/ while the
#       yuneta build only link_directories() outputs_ext/lib (see
#       tools/cmake/project.cmake) — link failures for libpcre2-8.a /
#       libmbedtls.a. Forcing lib keeps a single outputs_ext/lib on every
#       distro. No-op on Debian (already lib). openssl already uses
#       --libdir=lib; liburing / ncurses / libbacktrace are autotools and
#       default to lib.
#   version 1.16
#       upgrade to openssl 3.6.3 (was 3.6.2): security patch release within
#       the 3.6 series, no API change. Fixes one High CVE (CVE-2026-45447,
#       heap use-after-free in PKCS7_verify) plus a batch of CMS / QUIC /
#       ASN.1 / AES CVEs. openssl is linked statically into every yuno, so
#       every yuno must be rebuilt + relinked to pick it up. Stayed on 3.6
#       (not 4.0) — same LTS rationale as 1.12.
#   version 1.17
#       upgrade to nginx release-1.31.2 (was release-1.30.2) to fix three
#       CVEs: CVE-2026-42530 (use-after-free in ngx_http_v3_module),
#       CVE-2026-42055 (buffer overflow in the HTTP/2 paths of
#       ngx_http_proxy_module / ngx_http_grpc_module) and CVE-2026-48142
#       (buffer overread in ngx_http_charset_module). Same pin-only bump
#       rationale as 1.12 / 1.13: nginx is a separate binary linked
#       dynamically against the host libssl/libpcre/libz (see version
#       1.10), so no yuneta consumer, header or CMakeLists change rides
#       along. NOTE: 1.31.x is the nginx *mainline* branch (odd minor),
#       not the 1.30.x stable line we were on — chosen because the fixes
#       landed there; revisit if a 1.30.x stable backport appears.
#       openresty (1.29.2.5) is a separate binary and is NOT covered by
#       this bump — track upstream openresty for a release that picks up
#       these patches before bumping it.
#   version 1.18
#       upgrade to openresty 1.31.1.1 (was 1.29.2.5): advances the
#       openresty-bundled nginx core from 1.29.2 to 1.31.1 (released
#       2026-05-29). Same pin-only / separate-dynamic-binary rationale as
#       v1.14 — no yuneta consumer change rides along; openresty links
#       dynamically against host libssl/libpcre/libz, so its bundled
#       OpenSSL 3.5.6 is irrelevant to our build.
#       CVE STATUS: nginx 1.31.1 core fixes CVE-2026-9256 (already had a
#       backport in 1.29.2.5). It does NOT cover the three CVEs fixed in
#       nginx 1.31.2 (2026-06-17): CVE-2026-42530 (http_v3 UAF),
#       CVE-2026-42055 (proxy/grpc HTTP/2 overflow) and CVE-2026-48142
#       (charset overread). openresty 1.31.1.1 was tagged 2026-05-29,
#       before nginx 1.31.2 — so the openresty binary remains exposed to
#       those three until upstream ships a 1.31.x.y based on >= nginx
#       1.31.2 (or backports). The standalone nginx binary IS patched
#       (pinned to release-1.31.2, v1.17). Track upstream openresty.

VERSION="1.18"


source ./repos2clone.sh
export CFLAGS="-Wno-error=char-subscripts -O3 -g -DNDEBUG" # let each library to be, or not
export CC=cc
export MAKEFLAGS="-j$(nproc)"

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

cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DJANSSON_BUILD_DOCS=OFF \
    -DJANSSON_EXAMPLES=OFF \
    -DJANSSON_WITHOUT_TESTS=ON \
    ..
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
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DUSE_SHARED_MBEDTLS_LIBRARY=Off \
    -DUSE_STATIC_MBEDTLS_LIBRARY=On \
    -DENABLE_TESTING=Off \
    -DENABLE_PROGRAMS=Off \
    ..
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
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DPCRE2_BUILD_PCRE2_16=ON \
    -DPCRE2_BUILD_PCRE2_32=ON \
    -DPCRE2_STATIC_PIC=ON \
    -DPCRE2_SUPPORT_JIT=ON \
    -DPCRE2_BUILD_PCRE2GREP=OFF \
    -DPCRE2_BUILD_TESTS=OFF \
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

./configure --prefix="${YUNETA_INSTALL_PREFIX}" \
    --disable-shared \
    --with-pic
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

cmake -DCMAKE_INSTALL_PREFIX:PATH="${YUNETA_INSTALL_PREFIX}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    ..
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
    --with-default-terminfo-dir=/yuneta/bin/ncurses/terminfo \
    --without-cxx --without-cxx-binding \
    --without-ada \
    --without-manpages \
    --without-tests \
    --without-progs \
    --without-debug \
    --with-pic \
    --enable-sp-funcs \
    --enable-widec
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
    --with-http_v2_module \
    --with-http_realip_module \
    --with-http_stub_status_module \
    --with-http_gzip_static_module \
    --with-stream \
    --with-stream_ssl_module \
    --with-threads \
    --with-file-aio \
    --with-pcre-jit
make
make install
cd ../..


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
    --with-http_v2_module \
    --with-http_realip_module \
    --with-http_stub_status_module \
    --with-http_gzip_static_module \
    --with-stream \
    --with-stream_ssl_module \
    --with-threads \
    --with-file-aio \
    --with-pcre-jit

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
