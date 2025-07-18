# Define an associative array with repositories and their corresponding versions
declare -A REPOS

#--------------------------------------------------------------------------
#   VERSIONS
# If you change some version of those libraries
#   remember to change the VERSION of installation in configure-libs.sh
#--------------------------------------------------------------------------
TAG_JANSSON="v2.14.1"
TAG_LIBURING="liburing-2.11"
TAG_MBEDTLS="v3.6.4"
TAG_OPENSSL="openssl-3.4.1"
TAG_PCRE2="pcre2-10.45"
TAG_OPENRESTY="1.27.1.2"    # warning: without the initial 'v'
TAG_ARGP_STANDALONE="v1.1.5"
TAG_NCURSES="v6.3"
TAG_NGINX="release-1.29.0"

#TAG_LIBJWT="v1.17.2"
#TAG_LIBJWT="v2.1.2"
#TAG_LIBJWT="v3.2.1"

#------------------------------------------
#   RESPOS
#------------------------------------------
# Add repositories and their versions (branch, tag, or commit hash)
REPOS["https://github.com/akheron/jansson.git"]="$TAG_JANSSON"
REPOS["https://github.com/axboe/liburing.git"]="$TAG_LIBURING"
REPOS["https://github.com/Mbed-TLS/mbedtls.git"]="$TAG_MBEDTLS"
REPOS["https://github.com/openssl/openssl.git"]="$TAG_OPENSSL"
REPOS["https://github.com/PCRE2Project/pcre2.git"]="$TAG_PCRE2"
REPOS["https://github.com/openresty/openresty.git"]="$TAG_OPENRESTY"
REPOS["https://github.com/ianlancetaylor/libbacktrace"]=""
REPOS["https://github.com/artgins/argp-standalone.git"]="$TAG_ARGP_STANDALONE"
REPOS["https://github.com/mirror/ncurses.git"]="$TAG_NCURSES"
REPOS["https://github.com/nginx/nginx.git"]="$TAG_NGINX"
