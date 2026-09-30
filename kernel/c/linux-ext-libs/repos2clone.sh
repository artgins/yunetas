# Define an associative array with repositories and their corresponding versions
declare -A REPOS

#--------------------------------------------------------------------------
#   VERSIONS
# If you change some version of those libraries
#   remember to change the VERSION of installation in configure-libs.sh
#--------------------------------------------------------------------------
TAG_JANSSON="v2.15.1"
TAG_LIBURING="liburing-2.15"
TAG_MBEDTLS="v4.2.0"
TAG_OPENSSL="openssl-3.6.3"
TAG_PCRE2="pcre2-10.47"
TAG_ARGP_STANDALONE="v1.1.5"
TAG_NCURSES="v6.4"
TAG_OPENRESTY="1.31.1.1"    # nginx 1.31.1 core (was 1.29.2.5 / nginx 1.29.2); warning: without the initial 'v'
# openresty comes as its release tarball, not as a git clone: building from the
# git tag runs util/mirror-tarballs, which fetches ~45 modules as tarballs from
# github.com. The release tarball is the output of that same step, with the same
# module versions. When bumping TAG_OPENRESTY, update its sha256 too:
#   curl -sSL https://openresty.org/download/openresty-<tag>.tar.gz | sha256sum
SHA256_OPENRESTY="65b78baadd3f0984055de89bf13f4a1932e5bfe9c31932037a134ea2b1a0ce42"
TAG_NCURSES="v6.4"
TAG_NGINX="release-1.31.2"  # CVE-2026-42530/42055/48142 (was release-1.30.2); mainline branch

#------------------------------------------
#   REPOS
#------------------------------------------
# Add repositories and their versions (branch, tag, or commit hash)
REPOS["https://github.com/akheron/jansson.git"]="$TAG_JANSSON"
REPOS["https://github.com/axboe/liburing.git"]="$TAG_LIBURING"
REPOS["https://github.com/Mbed-TLS/mbedtls.git"]="$TAG_MBEDTLS"
REPOS["https://github.com/openssl/openssl.git"]="$TAG_OPENSSL"
REPOS["https://github.com/PCRE2Project/pcre2.git"]="$TAG_PCRE2"
REPOS["https://github.com/ianlancetaylor/libbacktrace"]=""
REPOS["https://github.com/artgins/argp-standalone.git"]="$TAG_ARGP_STANDALONE"
REPOS["https://github.com/mirror/ncurses.git"]="$TAG_NCURSES"
REPOS["https://github.com/nginx/nginx.git"]="$TAG_NGINX"
