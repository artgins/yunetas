#!/bin/bash
#
#   session-start.sh
#
#   SessionStart hook for Claude Code on the web: prepares a cloud container
#   so the yunetas C suite and the JS packages can be built and tested.
#
#   What a node gets from the .deb/.rpm, a fresh container lacks:
#       - the `yuneta` user and group (a yuno refuses to start for anybody
#         else, so the C tests must run as `yuneta`, never as root)
#       - /yuneta owned by `yuneta` (tests write under /yuneta/store)
#       - the inotify limits of 99-yuneta-core.conf (the Linux default of
#         128 instances per user is below what the treedb tests open)
#
#   Always done: OS packages and the `yunetas` CLI when missing, a default
#   .config when missing, the three points above, and `npm ci` of the JS
#   packages present that have no node_modules yet (ci: a plain install
#   rewrites package-lock.json with some npm versions). Each step is skipped
#   when its result is already there, so a container that was prepared once
#   starts fast.
#
#   It runs only at the start of a session (matcher "startup" in
#   .claude/settings.json), never on resume, clear or compact: `npm ci`
#   deletes node_modules, and doing that under a running vite or vitest
#   breaks it.
#
#   Done only with YUNETAS_HOOK_BUILD_C=1 (about 20 minutes, cached with the
#   container afterwards): the external libraries and `yunetas init/build`.
#   The hook timeout in .claude/settings.json (1800 s) leaves room for it.
#
#   Run the C suite afterwards as:
#       cd build && su yuneta -s /bin/bash -c 'ulimit -Sn 1024; ctest'
#
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

YUNETAS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${YUNETAS_DIR}"

#
#   OS packages and the yunetas CLI
#
export PATH="${HOME}/.local/bin:${PATH}"   # where pipx puts the CLI and kconfiglib
if ! command -v ninja >/dev/null 2>&1 || ! command -v pipx >/dev/null 2>&1; then
    ./install-dependencies.sh
fi
if ! command -v yunetas >/dev/null 2>&1; then
    pipx install yunetas
fi
if ! command -v alldefconfig >/dev/null 2>&1; then
    pipx install kconfiglib
fi

#
#   Default .config (the node configuration: static, OpenSSL, no memory tracking)
#
if [ ! -f .config ]; then
    alldefconfig
fi

#
#   The yuneta user, group and base directory
#
groupadd -f yuneta
if ! id yuneta >/dev/null 2>&1; then
    useradd -m -g yuneta yuneta
fi
mkdir -p /yuneta
# The checkouts stay the session's (git refuses a repo owned by another
# user); only their build/, where the tests run as yuneta, is yuneta's.
find /yuneta -xdev \( -path "${YUNETAS_DIR}" -o -path "${YUNETAS_DIR}/../gobj-js" \
        -o -path "${YUNETAS_DIR}/../gobj-ui.js" \) -prune -o \
    \( ! -user yuneta -o ! -group yuneta \) -exec chown -h yuneta:yuneta {} +
if [ -d "${YUNETAS_DIR}/build" ]; then
    chown -R yuneta:yuneta "${YUNETAS_DIR}/build"
fi

#
#   inotify limits, the same values as 99-yuneta-core.conf
#
if ! sysctl -q -w fs.inotify.max_user_instances=4096 \
        fs.inotify.max_user_watches=524288 \
        fs.inotify.max_queued_events=65536; then
    echo "WARNING: cannot raise the inotify limits; some treedb tests will fail" >&2
fi

#
#   JS packages: the submodules, or sibling clones of the standalone repos
#
for d in kernel/js/gobj-js kernel/js/gobj-ui ../gobj-js ../gobj-ui.js; do
    if [ -f "${d}/package.json" ] && [ ! -d "${d}/node_modules" ]; then
        (cd "${d}" && npm ci --no-audit --no-fund)
    fi
done

#
#   Optional: external libraries and the SDK
#
if [ "${YUNETAS_HOOK_BUILD_C:-0}" = "1" ]; then
    # shellcheck disable=SC1091
    source ./yunetas-env.sh >/dev/null
    # Rebuilt when missing or when configure-libs.sh says another VERSION:
    # a container cached on an older one would make `yunetas build` refuse.
    WANTED="$(sed -n 's/^VERSION="\(.*\)"$/\1/p' kernel/c/linux-ext-libs/configure-libs.sh | tail -1)"
    INSTALLED="$(head -1 kernel/c/linux-ext-libs/VERSION_INSTALLED.txt 2>/dev/null || true)"
    if [ "${INSTALLED}" != "${WANTED}" ]; then
        (cd kernel/c/linux-ext-libs && ./extrae.sh && ./configure-libs.sh)
    fi
    yunetas init
    yunetas build
    cmake --build build -j"$(nproc)"
    chown -R yuneta:yuneta build
fi
