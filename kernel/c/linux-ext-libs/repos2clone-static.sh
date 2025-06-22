# Define an associative array with repositories and their corresponding versions
declare -A REPOS

#--------------------------------------------------------------------------
#   VERSIONS
# If you change some version of those libraries
#   remember to change the VERSION of installation in configure-libs.sh
#--------------------------------------------------------------------------
TAG_JANSSON="v2.14.1"
TAG_LIBURING="liburing-2.11"
TAG_MBEDTLS="v3.6.2"    # last versions failing
TAG_OPENSSL="openssl-3.4.1"
TAG_PCRE2="pcre2-10.45"
TAG_LIBJWT="v3.2.1"
TAG_ARGP_STANDALONE="v1.1.5"

#TAG_LIBJWT="x3.2.1a"
#REPOS["https://github.com/artgins/libjwt.git"]="$TAG_LIBJWT"

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
REPOS["https://github.com/benmcollins/libjwt.git"]="$TAG_LIBJWT"
REPOS["https://github.com/ianlancetaylor/libbacktrace"]=""
REPOS["https://github.com/artgins/argp-standalone.git"]="$TAG_ARGP_STANDALONE"
