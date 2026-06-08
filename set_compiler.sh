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

#----------------------------------------#
#       Reset compiler alternatives
#----------------------------------------#
#   Detect the distro family so the reset uses the right package manager.
#   On Debian the gcc/cc alternatives are wiped and the packages
#   reinstalled to restore the stock links; on RHEL gcc/cc are plain
#   files (not managed by alternatives), so only the reinstall applies.
DISTRO_FAMILY="unknown"
if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    case " ${ID:-} ${ID_LIKE:-} " in
        *" debian "*|*" ubuntu "*) DISTRO_FAMILY="debian" ;;
        *" rhel "*|*" fedora "*|*" centos "*) DISTRO_FAMILY="rhel" ;;
    esac
fi

if [[ "$DISTRO_FAMILY" == "debian" ]]; then
    sudo update-alternatives --remove-all gcc 2>/dev/null || true
    sudo update-alternatives --remove-all cc 2>/dev/null || true
    sudo apt reinstall -y gcc clang
elif [[ "$DISTRO_FAMILY" == "rhel" ]]; then
    sudo dnf -y reinstall gcc clang || true
else
    echo "⚠️  Unknown distro family; skipping compiler-package reset." >&2
fi

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
        sudo update-alternatives --install "$alt_link" "$alt" "$path" 100
    fi

    sudo update-alternatives --set "$alt" "$path"
    echo "✅ Setting $alt to $path"
}

# Register and set for cc and gcc
register_and_set cc "$CC_PATH"

echo "🧪 Compiler links:"
echo "  cc  -> $(cc --version)"

printf "\n❗\033[1;31mREMEMBER\033[0m❗: Now you must execute \033[1;33myunetas init\033[0m\n\n"
