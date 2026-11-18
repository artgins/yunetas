# Define an associative array with repositories and their corresponding versions
declare -A REPOS

#--------------------------------------------------------------------------
#   VERSIONS
# If you change some version of those libraries
#   remember to change the VERSION of installation in configure-libs.sh
#--------------------------------------------------------------------------
# jansson is Yuneta's own (artgins/jansson): upstream v2.15.1 with Yuneta's
# series on master, pinned by its annotated tag (see HACKS.md)
# -yuneta.2: no -Wstringop-overflow at -O3 in stream_get() (same code paths)
TAG_JANSSON="2.15.1-yuneta.2"
TAG_LIBURING="liburing-2.15"
TAG_MBEDTLS="v4.3.0"
TAG_OPENSSL="openssl-3.6.5"
TAG_PCRE2="pcre2-10.49"
TAG_ARGP_STANDALONE="v1.1.5"
TAG_NCURSES="v6.4"
TAG_OPENRESTY="1.31.8.1"    # nginx 1.31.8 core (was 1.31.1.1 / nginx 1.31.1); warning: without the initial 'v'
# openresty comes as its release tarball, not as a git clone: building from the
# git tag runs util/mirror-tarballs, which fetches ~45 modules as tarballs from
# github.com. The release tarball is the output of that same step, with the same
# module versions. When bumping TAG_OPENRESTY, update its sha256 too:
#   curl -sSL https://openresty.org/download/openresty-<tag>.tar.gz | sha256sum
SHA256_OPENRESTY="b569c63b420f80a4ac8466f2deffe73ed93ab760026532e039c3658409a997f4"
TAG_NCURSES="v6.4"
TAG_NGINX="release-1.31.9"  # ten CVEs fixed (was release-1.31.8); mainline branch

#------------------------------------------
#   REPOS
#------------------------------------------
# Add repositories and their versions (branch, tag, or commit hash)
REPOS["https://github.com/artgins/jansson.git"]="$TAG_JANSSON"
REPOS["https://github.com/axboe/liburing.git"]="$TAG_LIBURING"
REPOS["https://github.com/Mbed-TLS/mbedtls.git"]="$TAG_MBEDTLS"
REPOS["https://github.com/openssl/openssl.git"]="$TAG_OPENSSL"
REPOS["https://github.com/PCRE2Project/pcre2.git"]="$TAG_PCRE2"
REPOS["https://github.com/ianlancetaylor/libbacktrace"]=""
REPOS["https://github.com/artgins/argp-standalone.git"]="$TAG_ARGP_STANDALONE"
REPOS["https://github.com/mirror/ncurses.git"]="$TAG_NCURSES"
REPOS["https://github.com/nginx/nginx.git"]="$TAG_NGINX"

#------------------------------------------
#   git_with_retry
#------------------------------------------
# GitHub answers 401 to anonymous clones of public repos when one address
# pulls too much at once (a whole extrae.sh, several nodes together); it
# clears within minutes. git's own retry of a submodule is immediate and hits
# the same limit, so wait between tries. A failed clone removes its directory
# and a submodule update resumes, so the same command is simply run again.
#   git_with_retry <git arguments...>
git_with_retry() {
    local waits=(30 90 180)
    local n=0
    while true; do
        if git "$@"; then
            return 0
        fi
        if [ "$n" -ge "${#waits[@]}" ]; then
            echo "[!] 'git $*' failed $((n + 1)) times; giving up." >&2
            return 1
        fi
        echo "[!] 'git $*' failed (try $((n + 1))); retrying in ${waits[$n]}s..." >&2
        sleep "${waits[$n]}"
        n=$((n + 1))
    done
}
