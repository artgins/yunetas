#!/usr/bin/env bash
#
#   audit-agents.sh
#
#   Say whether the two agents of this node are running THE BINARIES THAT
#   ARE ON DISK. It changes nothing.
#
#   Every node runs `yuneta_agent` plus `yuneta_agent22`, a deliberate
#   redundancy: each can upgrade the other, and the spare is the only way
#   into a node whose main agent is broken. That is why they are never
#   stopped or upgraded at once -- and why a spare left behind on an old
#   binary is invisible until the day it is needed.
#
#   It happened: five days on four nodes, running the version-comparison
#   bug that 7.12.0 fixed. `install.sh` restarts the spare after a package
#   upgrade, so the RUNTIME nodes are covered; a node that BUILDS FROM
#   SOURCE has no install.sh, `yunetas build` replaces /yuneta/agent/* and
#   both agents are restarted by hand. Nothing checked, and nothing said.
#
#   THE TEST IS ONE LINE, AND IT IS EXACT -- it reads the PROCESS and not
#   the file:
#
#       readlink /proc/<pid>/exe   ends in " (deleted)"
#
#   That is enough because Linux refuses to write into a binary that is
#   being executed (ETXTBSY): `cp` over a running agent fails outright,
#   and `install` / `mv` / a package succeed only by UNLINKING first, which
#   leaves the running process holding an inode with no name. So every
#   replacement that can happen while the agent runs is one the kernel
#   marks for us. There is no in-place case to miss.
#
#   Levels:
#       FAIL  an agent is not running at all.
#       WARN  an agent is running a binary that is no longer on disk.
#       INFO  worth knowing before deciding.
#       OK    shown only with --all.
#
#   Exit status: 2 if any FAIL, 1 if any WARN, 0 otherwise.
#
#   Usage:
#       audit-agents.sh          what is wrong
#       audit-agents.sh --all    also what is right
#
set -uo pipefail

AGENT_DIR=/yuneta/agent

N_FAIL=0
N_WARN=0
SHOW_OK=0

#
#   Helpers
#
usage() {
    cat <<'EOF'
Usage:
    audit-agents.sh          what is wrong
    audit-agents.sh --all    also what is right
EOF
}

fail() {
    printf 'FAIL  %s\n' "$*"
    N_FAIL=$((N_FAIL + 1))
}

warn() {
    printf 'WARN  %s\n' "$*"
    N_WARN=$((N_WARN + 1))
}

info() {
    printf 'INFO  %s\n' "$*"
}

ok() {
    if [ "$SHOW_OK" -eq 1 ]; then
        printf 'OK    %s\n' "$*"
    fi
}

#
#   One agent: is it running, and is it running what is on disk?
#
audit_agent() {
    local name=$1
    local role=$2          # main | spare
    local bin="$AGENT_DIR/$name"
    local pids pid exe stale=0 unreadable=0 version

    if [ ! -x "$bin" ]; then
        fail "$name: no binary at $bin"
        return
    fi

    #  By NAME and not by command line: a `pgrep -f` pattern matches the
    #  shell running this script too, because its own command line holds
    #  the pattern. Several pids are expected -- an agent is a watcher
    #  plus the process it watches.
    pids=$(pgrep -x "$name" 2>/dev/null)
    if [ -z "$pids" ]; then
        if [ "$role" = "spare" ]; then
            warn "$name: NOT RUNNING. The spare is the only way into a node whose main agent is broken."
        else
            fail "$name: NOT RUNNING."
        fi
        return
    fi

    for pid in $pids; do
        exe=$(readlink "/proc/$pid/exe" 2>/dev/null)
        if [ -z "$exe" ]; then
            unreadable=1
            continue
        fi
        case "$exe" in
            *' (deleted)')
                stale=1
                warn "$name (pid $pid): STALE. It runs a binary that is no longer on disk, so the file was replaced and this process was never restarted."
                ;;
        esac
    done

    if [ "$unreadable" -eq 1 ]; then
        info "$name: could not read /proc/<pid>/exe of every process. Run this as the user that owns the agents, or as root."
    fi

    if [ "$stale" -eq 1 ]; then
        version=$("$bin" --version 2>/dev/null | head -1)
        if [ -n "$version" ]; then
            info "$name: a restart would give you '$version'."
        fi
    elif [ "$unreadable" -eq 0 ]; then
        ok "$name: running the binary on disk (pids $(echo $pids))."
    fi
}

#
#   Main
#
main() {
    case "${1:-}" in
        --all)      SHOW_OK=1 ;;
        -h|--help)  usage; exit 0 ;;
        "")         ;;
        *)          usage; exit 1 ;;
    esac

    audit_agent yuneta_agent   main
    audit_agent yuneta_agent22 spare

    if [ "$N_WARN" -gt 0 ] || [ "$N_FAIL" -gt 0 ]; then
        cat <<'EOF'

An agent is a standalone daemon and NOT a managed yuno, so kill-yuno /
update-binary / run-yuno do nothing to it. It re-daemonizes on the new
binary with:

    /yuneta/agent/<name> --config-file=/yuneta/agent/<name>.json --stop
    /yuneta/agent/<name> --config-file=/yuneta/agent/<name>.json --start

ONE AT A TIME, the main agent first and the spare only once the main one
is confirmed healthy -- the order install.sh uses. Never both at once:
the spare is the way back into a node whose main agent will not come up.
EOF
    fi

    if [ "$N_FAIL" -gt 0 ]; then
        exit 2
    fi
    if [ "$N_WARN" -gt 0 ]; then
        exit 1
    fi
    exit 0
}

main "$@"
