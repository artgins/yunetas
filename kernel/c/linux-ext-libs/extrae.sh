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

#----------------------------------------#
#       Select compiler
#----------------------------------------#

CONFIG_FILE="$(dirname "$0")/../../../.config"

# Load selected compiler from config
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "❌ .config file not found: $CONFIG_FILE"
    exit 1
fi

if grep -q '^CONFIG_USE_COMPILER_CLANG=y' "$CONFIG_FILE"; then
    COMPILER="clang"
    PRIORITY=100
elif grep -q '^CONFIG_USE_COMPILER_GCC=y' "$CONFIG_FILE"; then
    COMPILER="gcc"
    PRIORITY=90
else
    echo "❌ No compiler selected in $CONFIG_FILE"
    exit 1
fi

# Resolve real path to compiler binary
CC_PATH=$(command -v "$COMPILER" || true)
if [[ -z "$CC_PATH" || ! -x "$CC_PATH" ]]; then
    echo "❌ Compiler binary '$COMPILER' not found in PATH"
    exit 1
fi

echo "========================================="
echo "🔧 Selecting compiler: $COMPILER ($CC_PATH)"
echo "========================================="

# Function to safely register and set a compiler alternative
register_and_set() {
    local alt="$1"
    local path="$2"
    local priority="$3"

    local alt_link="/usr/bin/$alt"
    local resolved_link resolved_target
    resolved_link=$(readlink -f "$alt_link" 2>/dev/null || true)
    resolved_target=$(readlink -f "$path" 2>/dev/null || true)

    # Skip if already pointing directly to the correct binary
    if [[ "$resolved_link" == "$resolved_target" ]]; then
        echo "⚠️  Skipping registration: $alt_link already points to $path"
        return
    fi

    # Register if not already known
    if ! update-alternatives --query "$alt" 2>/dev/null | grep -q "Value: $path"; then
        echo "➕ Registering $alt → $path"
        sudo update-alternatives --install "$alt_link" "$alt" "$path" "$priority"
    fi

    echo "✅ Setting $alt to $path"
    sudo update-alternatives --set "$alt" "$path"
}

# Register and set for cc and gcc
register_and_set cc "$CC_PATH" "$PRIORITY"
register_and_set gcc "$CC_PATH" "$PRIORITY"

echo "🧪 Compiler links:"
echo "  cc  -> $(readlink -f "$(command -v cc)")"
echo "  gcc -> $(readlink -f "$(command -v gcc)")"

#----------------------------------------#
#       Remove build
#----------------------------------------#
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
