#!/bin/bash

VERSION_OPENRESTY="v1.27.1.1"

sudo apt -y install libjansson-dev          # required for libjwt
sudo apt -y install libpcre2-dev            # required by openresty
sudo apt -y install perl dos2unix mercurial # required by openresty

sudo apt -y install kconfig-frontends       # required by yunetas, configuration tool

if ! command -v ldconfig >/dev/null 2>&1; then
    echo "ldconfig is not available in PATH. Exiting. Add /usr/sbin/ to PATH"
    exit 1
fi

if ! command -v meson >/dev/null 2>&1; then
    echo "meson is not available in PATH. Exiting. Run 'sudo apt install meson'"
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "ninja is not available in PATH. Exiting. Run 'sudo apt install ninja-build'"
    exit 1
fi


#  Exit immediately if a command exits with a non-zero status.
set -e

rm -rf build/
mkdir build
cd build

# Define an associative array with repositories and their corresponding versions
declare -A REPOS

# Add repositories and their versions (branch, tag, or commit hash)
REPOS["https://github.com/artgins/jansson-artgins.git"]="master"
REPOS["https://github.com/axboe/liburing.git"]="liburing-2.7"
REPOS["https://github.com/Mbed-TLS/mbedtls.git"]="v3.6.2"
REPOS["https://github.com/openssl/openssl.git"]="openssl-3.4.0"
REPOS["https://github.com/PCRE2Project/pcre2.git"]="pcre2-10.44"
REPOS["https://github.com/benmcollins/libjwt.git"]="v1.17.2"
REPOS["https://github.com/openresty/openresty.git"]="v1.27.1.1"

echo ""
echo "=======================CLONING====================="

# Loop through the list of repositories and clone each one
for REPO_URL in "${!REPOS[@]}"; do
    VERSION="${REPOS[$REPO_URL]}"

    # Extract the repository name from the URL
    REPO_NAME=$(basename -s .git "$REPO_URL")

    # Clone the repository with the specified version
    echo ""
    echo "===================> Cloning $REPO_NAME at version $VERSION from $REPO_URL"
    git clone --branch "$VERSION" --single-branch "$REPO_URL"

    # Optional: Print a message after each repository is cloned
    echo "<=================== Finished cloning $REPO_NAME"
    echo ""
done

# Optional: Print a final message
echo "All repositories have been cloned."

cd ..
