# Define an associative array with repositories and their corresponding versions
declare -A REPOS

#------------------------------------------
#   VERSIONS
#------------------------------------------
TAG_JANSSON="master"
TAG_LIBURING="liburing-2.7"
TAG_MBEDTLS="v3.6.2"
TAG_OPENSSL="openssl-3.4.0"
TAG_PCRE2="pcre2-10.44"
TAG_LIBJWT="v1.17.2"
TAG_OPENRESTY="1.27.1.1"    # warning: without the initial 'v'

#------------------------------------------
#   RESPOS
#------------------------------------------
# Add repositories and their versions (branch, tag, or commit hash)
REPOS["https://github.com/artgins/jansson-artgins.git"]="$TAG_JANSSON"
REPOS["https://github.com/axboe/liburing.git"]="$TAG_LIBURING"
REPOS["https://github.com/Mbed-TLS/mbedtls.git"]="$TAG_MBEDTLS"
REPOS["https://github.com/openssl/openssl.git"]="$TAG_OPENSSL"
REPOS["https://github.com/PCRE2Project/pcre2.git"]="$TAG_PCRE2"
REPOS["https://github.com/benmcollins/libjwt.git"]="$TAG_LIBJWT"
REPOS["https://github.com/openresty/openresty.git"]="$TAG_OPENRESTY"