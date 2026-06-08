#!/bin/bash
#
#   install-dependencies.sh
#
#   Install the OS packages needed to build Yunetas from source.
#
#   Supports two distro families, auto-detected from /etc/os-release:
#       - Debian / Ubuntu        (apt-get)
#       - RHEL / Rocky / Alma / Fedora / CentOS Stream  (dnf)
#
#   It does NOT build Yunetas. After this finishes, follow the
#   "Build from source" steps in docs/doc.yuneta.io/installation.md:
#       source yunetas-env.sh
#       ./set_compiler.sh
#       menuconfig
#       cd kernel/c/linux-ext-libs && ./extrae.sh && ./configure-libs.sh
#       yunetas init && yunetas build && yunetas test
#
#   Usage:
#       ./install-dependencies.sh            # install everything
#       ./install-dependencies.sh --no-sudo  # print the package manager
#                                            # command without running it
#
#   Re-runnable: installing already-present packages is a no-op.
#

set -euo pipefail

#----------------------------------------#
#   Parse args
#----------------------------------------#
RUN_SUDO=1
for arg in "$@"; do
    case "$arg" in
        --no-sudo)
            RUN_SUDO=0
            ;;
        -h|--help)
            grep '^#' "$0" | sed 's/^#//'
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

SUDO="sudo"
if [[ "$RUN_SUDO" -eq 0 || "$(id -u)" -eq 0 ]]; then
    SUDO=""
fi

#----------------------------------------#
#   Detect distro family
#----------------------------------------#
if [[ ! -r /etc/os-release ]]; then
    echo "❌ /etc/os-release not found; cannot detect the distribution." >&2
    exit 1
fi

# shellcheck disable=SC1091
. /etc/os-release

FAMILY=""
case " ${ID:-} ${ID_LIKE:-} " in
    *" debian "*|*" ubuntu "*)
        FAMILY="debian"
        ;;
    *" rhel "*|*" fedora "*|*" centos "*)
        FAMILY="rhel"
        ;;
esac

if [[ -z "$FAMILY" ]]; then
    echo "❌ Unsupported distribution: ID='${ID:-?}' ID_LIKE='${ID_LIKE:-?}'." >&2
    echo "   Supported families: Debian/Ubuntu and RHEL/Rocky/Alma/Fedora." >&2
    echo "   Install the equivalent of the package list in" >&2
    echo "   docs/doc.yuneta.io/installation.md by hand." >&2
    exit 1
fi

echo "================================================="
echo "🔧 Detected distribution: ${PRETTY_NAME:-$ID}  (family: $FAMILY)"
echo "================================================="

#----------------------------------------#
#   Debian / Ubuntu
#----------------------------------------#
if [[ "$FAMILY" == "debian" ]]; then
    DEB_PKGS=(
        git mercurial make cmake ninja-build
        gcc clang g++
        python3-dev python3-pip python3-setuptools
        python3-tk python3-wheel python3-venv
        libjansson-dev libpcre2-dev liburing-dev
        libpcre3-dev zlib1g-dev libssl-dev
        perl dos2unix tree curl wget
        postgresql-server-dev-all libpq-dev
        kconfig-frontends telnet pipx
        patch gettext fail2ban rsync
        build-essential pkg-config ca-certificates linux-libc-dev
    )

    echo "📦 Installing ${#DEB_PKGS[@]} packages with apt-get..."
    $SUDO apt-get update
    $SUDO apt-get -y install --no-install-recommends "${DEB_PKGS[@]}"
fi

#----------------------------------------#
#   RHEL / Rocky / Alma / Fedora
#----------------------------------------#
if [[ "$FAMILY" == "rhel" ]]; then
    #
    #   Several packages live in EPEL (mercurial, ninja-build, telnet,
    #   pipx, fail2ban, python3-wheel) and CRB / CodeReady Builder
    #   (liburing-devel). Enable both before installing.
    #
    #   Fedora ships everything in the main repos and has no CRB, so the
    #   enable steps are skipped there.
    #
    if [[ "${ID:-}" != "fedora" ]]; then
        echo "📦 Enabling EPEL..."
        $SUDO dnf -y install epel-release || true

        echo "📦 Enabling CRB (CodeReady Builder)..."
        if command -v crb >/dev/null 2>&1; then
            $SUDO crb enable || true
        else
            # Repo id is 'crb' on RHEL 9 / Rocky 9 / Alma 9,
            # 'powertools' on the EL8 line.
            $SUDO dnf config-manager --set-enabled crb 2>/dev/null \
                || $SUDO dnf config-manager --set-enabled powertools 2>/dev/null \
                || true
        fi
    fi

    #
    #   RHEL package names mapped from the Debian list. Notes:
    #     - jansson-devel / liburing-devel / pcre2-devel / openssl-devel
    #       are dev headers; Yunetas builds its own static copies of
    #       jansson/liburing/openssl/pcre2/ncurses under
    #       kernel/c/linux-ext-libs (outputs_ext/), so these are only
    #       needed for the dynamically-linked nginx/openresty and for
    #       convenience. They are kept to mirror the Debian list.
    #     - python3-venv has no separate package on RHEL; venv ships with
    #       python3.
    #     - kconfig-frontends has no RHEL package; menuconfig comes from
    #       'pipx install kconfiglib' below.
    #     - curl is already present as curl-minimal; not requested here to
    #       avoid the curl/curl-minimal swap prompt.
    #     - glibc-static / libstdc++-static / libxcrypt-static (CRB) provide
    #       libc.a / libstdc++.a / libcrypt.a, needed for the default
    #       CONFIG_FULLY_STATIC=y build (-lc / -lstdc++ / -lcrypt). On Debian
    #       these static archives ship inside libc6-dev / build-essential.
    #
    RHEL_PKGS=(
        git mercurial make cmake ninja-build
        gcc clang gcc-c++
        python3-devel python3-pip python3-setuptools
        python3-tkinter python3-wheel
        jansson-devel pcre2-devel liburing-devel
        pcre-devel zlib-devel openssl-devel
        perl dos2unix tree wget
        libpq-devel
        telnet pipx
        patch gettext fail2ban rsync
        pkgconf-pkg-config ca-certificates glibc-devel kernel-headers
        glibc-static libstdc++-static libxcrypt-static
    )

    echo "📦 Installing ${#RHEL_PKGS[@]} packages with dnf..."
    $SUDO dnf -y install "${RHEL_PKGS[@]}"
fi

#----------------------------------------#
#   Common: menuconfig backend
#----------------------------------------#
echo "📦 Installing kconfiglib (menuconfig) via pipx..."
if command -v pipx >/dev/null 2>&1; then
    pipx install kconfiglib || pipx upgrade kconfiglib || true
    pipx ensurepath || true
else
    echo "⚠️  pipx not on PATH; install kconfiglib manually:" >&2
    echo "    python3 -m pip install --user kconfiglib" >&2
fi

#----------------------------------------#
#   RHEL: warn if io_uring is disabled
#----------------------------------------#
#   Yuneta's event loop (yev_loop) is io_uring-based. RHEL/Rocky/Alma 9
#   ship kernel.io_uring_disabled=2 (fully disabled), so every yuno aborts
#   at startup. We only WARN here — re-enabling it is a host security
#   decision left to the operator, not something a dep installer should do
#   silently.
if [[ "$FAMILY" == "rhel" ]]; then
    IOURING_STATE=$(sysctl -n kernel.io_uring_disabled 2>/dev/null || echo "")
    if [[ "$IOURING_STATE" == "1" || "$IOURING_STATE" == "2" ]]; then
        echo ""
        echo "⚠️  io_uring is disabled (kernel.io_uring_disabled=$IOURING_STATE)."
        echo "    Yuneta's event loop needs io_uring. Enable it before running:"
        echo "      echo 'kernel.io_uring_disabled = 0' | sudo tee /etc/sysctl.d/99-yuneta-iouring.conf"
        echo "      sudo sysctl --system"
    fi
fi

echo ""
echo "✅ Dependencies installed."
echo "   Next: source yunetas-env.sh && ./set_compiler.sh && menuconfig"
