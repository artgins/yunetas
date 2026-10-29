#!/bin/bash
#
#   The cloners -- yclone-gclass, yclone-project and ymake-skeleton, one
#   engine (utils/c/yclone-project/clone_tree_dir.c) -- on a yuno made by
#   yuno-skeleton. Each check is a bug they had up to 7.28.6:
#
#     - yclone-gclass time -> clock: C_TIMER, set_timeout and EV_TIMEOUT are
#       left as they were (a key is replaced as a WHOLE word; they became
#       C_CLOCKR, set_clockout, EV_CLOCKOUT), and the clone builds;
#     - yclone-project to a name that holds the old one (sktcln ->
#       sktcln_two): replaced once (it was sktcln_two_two); a binary copied
#       byte for byte (it was cut at its first NUL); the mode of a script
#       kept; .git and build not cloned; a symlink kept; the clone builds
#       and says its new role; a destination inside the source refused;
#     - ymake-skeleton makes a template that yuno-skeleton instantiates and
#       that builds (the round trip), with a directory named after the
#       rootname rendered too (it got a literal {{rootname}}).
#
#   And the ones of 7.29.1: a symlink to a file the clone renames points to
#   the new name, in both tools of the round trip (it was left dangling);
#   the round trip keeps the mode of a script (yuno-skeleton dropped it);
#   a destination path longer than PATH_MAX is an error that says so (the
#   truncated path was used).
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
set -u

#
#   The tree is the one this script is in (see test_yuno_skeletons.sh)
#
YUNETAS_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
export YUNETAS_BASE
SKELETON="$YUNETAS_BASE/utils/c/yuno-skeleton/build/yuno-skeleton"
SKELETONS="$YUNETAS_BASE/utils/c/yuno-skeleton/skeletons"
CLONE_GCLASS="$YUNETAS_BASE/utils/c/yclone-gclass/build/yclone-gclass"
CLONE_PROJECT="$YUNETAS_BASE/utils/c/yclone-project/build/yclone-project"
MAKE_SKELETON="$YUNETAS_BASE/utils/c/ymake-skeleton/build/ymake-skeleton"
WORK=$(mktemp -d /tmp/test_cloners.XXXXXX)
[ -n "${KEEP_WORK:-}" ] || trap 'rm -rf "$WORK"' EXIT
FAILURES=0

#
#   Helpers
#
fail() {
    echo "FAIL $*"
    FAILURES=$((FAILURES+1))
}

generate() {    # dir skeleton answers [skeletons dir]
    (cd "$1" && printf "$3" | "$SKELETON" -p "${4:-$SKELETONS}" "$2") > "$WORK/gen_$2.log" 2>&1 \
        || fail "yuno-skeleton $2 (see $WORK/gen_$2.log)"
}

build() {       # project dir
    mkdir -p "$1/build"
    if ! (cd "$1/build" && cmake .. > cmake.log 2>&1 && make > make.log 2>&1); then
        fail "build of $(basename "$1")"
        tail -20 "$1/build/make.log"
        return 1
    fi
    if grep -q "warning:" "$1/build/make.log"; then
        fail "warnings in the build of $(basename "$1")"
        grep "warning:" "$1/build/make.log" | head
    fi
    return 0
}

#
#   Prerequisites
#
for bin in "$SKELETON" "$CLONE_GCLASS" "$CLONE_PROJECT" "$MAKE_SKELETON"; do
    if [ ! -x "$bin" ]; then
        echo "FAIL not built: $bin"
        exit 1
    fi
done

#
#   The source: a standalone yuno (role sktcln, root sktroot) with a
#   gclass_service named `time`, built (so it has a build/), a git repo, a
#   binary, an executable script, a symlink and a directory named after the
#   root
#
generate "$WORK" yuno_standalone "sktcln\nsktroot\n1.0.0\nClone test\nArtGins\nsupport@artgins.com\nMIT\n"
SRC="$WORK/sktcln"
generate "$SRC/src" gclass_service "time\nTime test\nArtGins\n"

#
#   yclone-gclass time -> clock
#
(cd "$SRC/src" && "$CLONE_GCLASS" time clock) > "$WORK/clone_gclass.log" 2>&1 \
    || fail "yclone-gclass time clock (see $WORK/clone_gclass.log)"
C="$SRC/src/c_clock.c"
if [ -f "$C" ] && [ -f "$SRC/src/c_clock.h" ]; then
    grep -q "GOBJ_DEFINE_GCLASS(C_CLOCK);" "$C" || fail "yclone-gclass: C_TIME not renamed to C_CLOCK"
    grep -q "register_c_clock(void)" "$C" || fail "yclone-gclass: register_c_time not renamed"
    grep -q "gobj_create_pure_child(gobj_name(gobj), C_TIMER," "$C" || fail "yclone-gclass: C_TIMER was touched"
    grep -q "set_timeout(priv->timer" "$C" || fail "yclone-gclass: set_timeout was touched"
    grep -q "{EV_TIMEOUT," "$C" || fail "yclone-gclass: EV_TIMEOUT was touched"
    grep -qE "^\s+C_TIMER\s+[0-9]+" "$WORK/clone_gclass.log" || fail "yclone-gclass: C_TIMER not reported as left as it was"
else
    fail "yclone-gclass did not make c_clock.c/.h"
fi
sed -i 's|^\(\s*\)src/c_sktroot.c$|\1src/c_sktroot.c\n\1src/c_time.c\n\1src/c_clock.c|' "$SRC/CMakeLists.txt"
grep -q "src/c_clock.c" "$SRC/CMakeLists.txt" || fail "the gclass sources were not added to the yuno"
build "$SRC"

(cd "$SRC" && git init -q && git add -A && git -c user.name=t -c user.email=t@t commit -qm init) \
    || fail "git init of the source"
cp "$SRC/build/sktcln" "$SRC/sktcln.bin"
printf '#!/bin/sh\necho sktcln\n' > "$SRC/run_sktcln.sh"
chmod 0755 "$SRC/run_sktcln.sh"
ln -s README.md "$SRC/README.link"
ln -s run_sktcln.sh "$SRC/run.link"
mkdir -p "$SRC/sktroot_data"
printf 'data of sktroot\n' > "$SRC/sktroot_data/sktroot.txt"

#
#   yclone-project sktcln -> sktcln_two
#
DST="$WORK/sktcln_two"
"$CLONE_PROJECT" "$SRC" "$DST" -s sktcln -d sktcln_two > "$WORK/clone_project.log" 2>&1 \
    || fail "yclone-project (see $WORK/clone_project.log)"
if [ -d "$DST" ]; then
    grep -rq "sktcln_two_two" "$DST" && fail "yclone-project replaced twice: sktcln_two_two"
    find "$DST" -name "*sktcln_two_two*" | grep -q . && fail "yclone-project replaced twice in a name"
    grep -q '#define APP_NAME        "sktcln_two"' "$DST/src/main.c" || fail "yclone-project: APP_NAME not renamed"
    [ -f "$DST/sktcln_two.bin" ] || fail "yclone-project: the binary was not cloned (or not renamed)"
    cmp -s "$SRC/sktcln.bin" "$DST/sktcln_two.bin" || fail "yclone-project: the binary is not byte for byte"
    [ -x "$DST/run_sktcln_two.sh" ] || fail "yclone-project: the script lost its mode"
    [ -e "$DST/.git" ] && fail "yclone-project cloned .git"
    [ -e "$DST/build" ] && fail "yclone-project cloned build"
    [ "$(readlink "$DST/README.link" 2>/dev/null)" = "README.md" ] || fail "yclone-project: the symlink was not kept"
    [ "$(readlink "$DST/run.link" 2>/dev/null)" = "run_sktcln_two.sh" ] \
        || fail "yclone-project: a symlink to a renamed file does not follow it"
    [ -e "$DST/run.link" ] || fail "yclone-project: a symlink left dangling"
    if build "$DST"; then
        "$DST/build/sktcln_two" --print-role 2>/dev/null | grep -q '"role": "sktcln_two"' \
            || fail "the cloned yuno does not say its new role"
    fi
else
    fail "yclone-project did not make $DST"
fi

if "$CLONE_PROJECT" "$SRC" "$SRC/inner" -s sktcln -d inner > "$WORK/clone_inner.log" 2>&1; then
    fail "yclone-project accepted a destination inside the source"
fi
[ -e "$SRC/inner" ] && fail "yclone-project created a destination inside the source"

#
#   ymake-skeleton, and the round trip through yuno-skeleton
#
SK="$WORK/skeletons"
mkdir -p "$SK"
"$MAKE_SKELETON" "$SRC" "$SK/skt_clone" --yunorole=sktcln --rootname=sktroot > "$WORK/make_skeleton.log" 2>&1 \
    || fail "ymake-skeleton (see $WORK/make_skeleton.log)"
if [ -d "$SK/skt_clone" ]; then
    [ -f "$SK/skt_clone/src/c_+rootname+.c_tmpl" ] || fail "ymake-skeleton: no src/c_+rootname+.c_tmpl"
    [ -f "$SK/skt_clone/src/main.c_tmpl" ] || fail "ymake-skeleton: no src/main.c_tmpl"
    grep -q '#define APP_NAME        "{{yunorole}}"' "$SK/skt_clone/src/main.c_tmpl" \
        || fail "ymake-skeleton: the role is not {{yunorole}} in main.c"
    grep -q "C_{{ROOTNAME}}" "$SK/skt_clone/src/main.c_tmpl" || fail "ymake-skeleton: no C_{{ROOTNAME}}"
    [ -d "$SK/skt_clone/+rootname+_data" ] || fail "ymake-skeleton: the directory name was not rendered as +rootname+"
    [ -f "$SK/skt_clone/+yunorole+.bin" ] || fail "ymake-skeleton: the binary got a suffix or was not renamed"
    cmp -s "$SRC/sktcln.bin" "$SK/skt_clone/+yunorole+.bin" || fail "ymake-skeleton: the binary is not byte for byte"
    [ -e "$SK/skt_clone/run.link" ] || fail "ymake-skeleton: a symlink to a renamed file left dangling"
    [ -e "$SK/skt_clone/README.link" ] || fail "ymake-skeleton: a symlink to a file that got the suffix left dangling"

    python3 - "$SKELETONS/__skeletons__.json" "$SK/__skeletons__.json" <<'PY'
import json, sys
entries = json.load(open(sys.argv[1]))
entry = dict(next(e for e in entries if e["name"] == "yuno_standalone"))
entry["name"] = "skt_clone"
json.dump([entry], open(sys.argv[2], "w"), indent=4)
PY
    mkdir -p "$WORK/round"
    generate "$WORK/round" skt_clone "sktrnd\nsktrndroot\n1.0.0\nRound trip\nArtGins\nsupport@artgins.com\nMIT\n" "$SK"
    R="$WORK/round/sktrnd"
    if [ -f "$R/src/c_sktrndroot.c" ]; then
        grep -rq "{{\|sktcln\|sktroot" "$R/src" "$R/CMakeLists.txt" && fail "the round trip left a marker or an old name"
        [ -d "$R/sktrndroot_data" ] || fail "the round trip did not render the directory name"
        [ -x "$R/run_sktrnd.sh" ] || fail "the round trip lost the mode of a script"
        [ "$(readlink "$R/run.link" 2>/dev/null)" = "run_sktrnd.sh" ] && [ -e "$R/run.link" ] \
            || fail "the round trip left a symlink dangling"
        [ -e "$R/README.link" ] || fail "the round trip left the README symlink dangling"
        if build "$R"; then
            "$R/build/sktrnd" --print-role 2>/dev/null | grep -q '"role": "sktrnd"' \
                || fail "the round-trip yuno does not say its role"
        fi
    else
        fail "the round trip did not make src/c_sktrndroot.c (see $WORK/gen_skt_clone.log)"
    fi
else
    fail "ymake-skeleton did not make $SK/skt_clone"
fi

#
#   A destination path longer than PATH_MAX: an error that says so, not a
#   truncated path used for the clone
#
SEG="$(printf 'd%.0s' $(seq 1 200))"
LP="$WORK/long/src"
for i in $(seq 1 19); do
    LP="$LP/$SEG"
done
mkdir -p "$LP" && printf 'a\n' > "$LP/f.txt"
LDST="$WORK/long/$(printf 'e%.0s' $(seq 1 250))"
if "$CLONE_PROJECT" "$WORK/long/src" "$LDST" -s a -d b > "$WORK/clone_long.log" 2>&1; then
    fail "yclone-project cloned to a path longer than PATH_MAX"
fi
grep -q "too long" "$WORK/clone_long.log" \
    || fail "yclone-project did not say the path is too long (see $WORK/clone_long.log)"

if [ $FAILURES -ne 0 ]; then
    echo "<-- TEST FAILED: yuno_skeleton/cloners ($FAILURES)"
    exit 1
fi
echo "ok   yuno_skeleton/cloners"
exit 0
