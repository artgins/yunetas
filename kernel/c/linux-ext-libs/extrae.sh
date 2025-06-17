#!/bin/bash

source ./repos2clone.sh

if ! command -v ldconfig >/dev/null 2>&1; then
    echo "ldconfig is not available in PATH. Exiting. Add /usr/sbin/ to PATH"
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "ninja is not available in PATH. Exiting. Run 'sudo apt install ninja-build'"
    exit 1
fi


#  Exit immediately if a command exits with a non-zero status.
set -e

#!/bin/bash
set -e

# Path to .config file (relative to where this script is located)
CONFIG_FILE="$(dirname "$0")/../../../.config"

# Helper to check if a config option is enabled
config_enabled() {
    grep -q "^$1=y" "$CONFIG_FILE"
}

# Detect selected compiler
if config_enabled CONFIG_USE_COMPILER_CLANG; then
    CC_PATH="/usr/bin/clang"
    NAME="clang"
    PRIORITY=100
elif config_enabled CONFIG_USE_COMPILER_GCC; then
    CC_PATH="/usr/bin/gcc"
    NAME="gcc"
    PRIORITY=90
elif config_enabled CONFIG_USE_COMPILER_MUSL; then
    CC_PATH="/usr/bin/musl-gcc"
    NAME="musl-gcc"
    PRIORITY=80
else
    echo "❌ No compiler selected in $CONFIG_FILE"
    exit 1
fi

echo "🔧 Selecting compiler: $NAME ($CC_PATH)"

# Register the alternative if needed
sudo update-alternatives --install /usr/bin/cc cc "$CC_PATH" "$PRIORITY" || true
sudo update-alternatives --install /usr/bin/gcc gcc "$CC_PATH" "$PRIORITY" || true

# Set it as default
sudo update-alternatives --set cc "$CC_PATH"
sudo update-alternatives --set gcc "$CC_PATH"

rm -rf build/
mkdir build
cd build


echo ""
echo "=======================CLONING====================="

# Loop through the list of repositories and clone each one
for REPO_URL in "${!REPOS[@]}"; do
    VERSION="${REPOS[$REPO_URL]}"

    # Extract the repository name from the URL
    REPO_NAME=$(basename -s .git "$REPO_URL")

    # Clone the repository with the specified version
    echo ""
    echo "===================> Cloning $REPO_NAME from $REPO_URL"
    git clone --recurse-submodules "$REPO_URL"

    # Optional: Print a message after each repository is cloned
    echo "<=================== Finished cloning $REPO_NAME"
    echo ""
done

# Optional: Print a final message
echo "All repositories have been cloned."

cd ..
