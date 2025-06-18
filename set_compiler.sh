#!/bin/bash

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if [[ -n "$YUNETAS_BASE" ]]; then
    YUNETAS_BASE_DIR="$YUNETAS_BASE"
else
    YUNETAS_BASE_DIR="/yuneta/development/yunetas"
fi

#sudo apt reinstall gcc musl musl-dev musl-tools clang
#sudo apt reinstall gcc

#----------------------------------------#
#       Select compiler
#----------------------------------------#
CONFIG_FILE="$YUNETAS_BASE_DIR/.config"

# Load selected compiler from config
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "❌ .config file not found: $CONFIG_FILE"
    echo "🔧 execute menuconfig in Yunetas directory to create .config file"
    exit 1
fi

if grep -q '^CONFIG_USE_COMPILER_CLANG=y' "$CONFIG_FILE"; then
    COMPILER="clang"
elif grep -q '^CONFIG_USE_COMPILER_GCC=y' "$CONFIG_FILE"; then
    COMPILER="gcc"
elif grep -q '^CONFIG_USE_COMPILER_MUSL=y' "$CONFIG_FILE"; then
    COMPILER="musl-gcc"
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

echo "================================================="
echo "🔧 Selecting compiler: $COMPILER ($CC_PATH)"
echo "================================================="

# Function to safely register and set a compiler alternative
register_and_set() {
    local alt="$1"
    local path="$2"

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
        sudo update-alternatives --install "$alt_link" "$alt" "$path"
    fi

    echo "✅ Setting $alt to $path"
    sudo update-alternatives --set "$alt" "$path"
}

# Register and set for cc and gcc
register_and_set cc "$CC_PATH"

echo "🧪 Compiler links:"
echo "  cc  -> $(readlink -f "$(command -v cc)")"

echo "  cc  -> $(cc --version)"
