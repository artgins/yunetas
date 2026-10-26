#!/bin/bash
#
#   The templates of yuno-skeleton must build: no ctest compiled them, and
#   two fixes of theirs (MSGSET_INTERNAL, the timer as a pure child) had no
#   test. Each template is instantiated with yuno-skeleton (its answers on
#   stdin, as a person types them) and built against the SDK in outputs/:
#
#     - yuno_standalone, with a gclass_child and a gclass_service generated
#       into its src/ and added to its sources; built with no warning, and
#       run three seconds: its gclass's timer (a pure child) fires
#       ("Timeout" on stdout); run again with the gclass_service as its
#       service and the gclass_child under it: each timer fires;
#     - yuno_citizen, with its SERVICE subscription block and its license,
#       built with no warning; c_project, built with no warning;
#     - js_gclass and js_gclass_service, loaded by node against gobj-js:
#       each one's timer fires;
#     - project and project_commercial, with a citizen yuno in yunos/,
#       built from the project root;
#     - no template holds a {{marker}} that nothing fills.
#
#   Copyright (c) 2026, ArtGins.
#   All Rights Reserved.
#
set -u

#
#   The tree is the one this script is in: a run in a worktree tests the
#   worktree, whatever YUNETAS_BASE the shell carries (it fell back to
#   /yuneta/development/yunetas and to the installed /yuneta/bin/yuno-skeleton,
#   so a worktree run without YUNETAS_BASE tested the live tree)
#
YUNETAS_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
export YUNETAS_BASE
BIN="$YUNETAS_BASE/utils/c/yuno-skeleton/build/yuno-skeleton"   # as built in this tree
SKELETONS="$YUNETAS_BASE/utils/c/yuno-skeleton/skeletons"
WORK=$(mktemp -d /tmp/test_yuno_skeletons.XXXXXX)
[ -n "${KEEP_WORK:-}" ] || trap 'rm -rf "$WORK"' EXIT
FAILURES=0

#
#   Helpers
#
fail() {
    echo "FAIL $*"
    FAILURES=$((FAILURES+1))
}

generate() {    # dir skeleton answers
    (cd "$1" && printf "$3" | "$BIN" -p "$SKELETONS" "$2") > "$WORK/gen_$2.log" 2>&1 \
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
if [ ! -x "$BIN" ]; then
    echo "FAIL yuno-skeleton not built: $BIN"
    exit 1
fi

#
#   yuno_standalone, with a gclass_child and a gclass_service
#
generate "$WORK" yuno_standalone "sktstd\n\n1.0.0\nSkeleton test\nArtGins\nsupport@artgins.com\nMIT\n"
generate "$WORK/sktstd/src" gclass_child "sktchild\nChild test\nArtGins\n"
generate "$WORK/sktstd/src" gclass_service "sktsvc\nService test\nArtGins\n"
for f in c_sktchild.c c_sktchild.h c_sktsvc.c c_sktsvc.h; do
    [ -f "$WORK/sktstd/src/$f" ] || fail "gclass skeleton did not make src/$f"
done
sed -i 's|^\(\s*\)src/c_sktstd.c$|\1src/c_sktstd.c\n\1src/c_sktchild.c\n\1src/c_sktsvc.c|' "$WORK/sktstd/CMakeLists.txt"
grep -q "src/c_sktsvc.c" "$WORK/sktstd/CMakeLists.txt" || fail "the gclass sources were not added to the standalone yuno"

if build "$WORK/sktstd"; then
    timeout -s INT 3 "$WORK/sktstd/build/sktstd" -l 1 > "$WORK/run.log" 2>&1
    grep -q "^Timeout" "$WORK/run.log" || { fail "the standalone yuno's timer did not fire"; head -20 "$WORK/run.log"; }
fi

#
#   The same yuno with the gclass_service as its service, and the
#   gclass_child as its child: each one's timer (a pure child) reaches its
#   ac_timeout. Up to 1cab6b1cc the timer of those templates was a plain
#   child, and its timeout never did.
#
MAIN="$WORK/sktstd/src/main.c"
python3 - "$MAIN" <<'PY'
import sys
path = sys.argv[1]
s = open(path).read()
s = s.replace('#include "c_sktstd.h"\n',
    '#include "c_sktstd.h"\n#include "c_sktsvc.h"\n#include "c_sktchild.h"\n')
s = s.replace('    result += register_c_sktstd();\n',
    '    result += register_c_sktstd();\n    result += register_c_sktsvc();\n    result += register_c_sktchild();\n')
s = s.replace("'gclass': 'C_SKTSTD',", "'gclass': 'C_SKTSVC',")
# The service played (its timer is armed at the play), and the child as a
# second service, started (its timer is armed at the start)
s = s.replace("'autoplay': false",
    "'autoplay': true                                         \\n\\\n"
    "        },                                                          \\n\\\n"
    "        {                                                           \\n\\\n"
    "            'name': 'child',                                        \\n\\\n"
    "            'gclass': 'C_SKTCHILD',                                 \\n\\\n"
    "            'autostart': true", 1)
open(path, "w").write(s)
PY
sed -i 's|printf("Timeout\\n");|printf("Timeout child\\n");|' "$WORK/sktstd/src/c_sktchild.c"
grep -q "register_c_sktsvc();" "$MAIN" && grep -q "'gclass': 'C_SKTSVC'" "$MAIN" && grep -q "C_SKTCHILD" "$MAIN" \
    || fail "the service and the child were not put in the standalone yuno"
if build "$WORK/sktstd"; then
    timeout -s INT 3 "$WORK/sktstd/build/sktstd" -l 1 > "$WORK/run2.log" 2>&1
    grep -q "^Timeout$" "$WORK/run2.log" || { fail "the gclass_service's timer did not reach its ac_timeout"; head -20 "$WORK/run2.log"; }
    grep -q "^Timeout child$" "$WORK/run2.log" || { fail "the gclass_child's timer did not reach its ac_timeout"; head -20 "$WORK/run2.log"; }
fi

#
#   yuno_citizen
#
#   Its root gclass is the yuno's default service, so it carries the SERVICE
#   subscription block (up to 7.28.6 both blocks were commented out under a
#   TODO); its README says the license asked for, and no empty name.
#
generate "$WORK" yuno_citizen "sktcit\n\n1.0.0\nSkeleton test\nArtGins\nsupport@artgins.com\nMIT\n"
grep -q "^    } else if(gobj_is_pure_child(gobj)) {$" "$WORK/sktcit/src/c_sktcit.c" \
    || fail "yuno_citizen: no SERVICE subscription block in c_sktcit.c"
grep -q "^MIT$" "$WORK/sktcit/README.md" || fail "yuno_citizen: the license is not in README.md"
grep -q "^Name: *$" "$WORK/sktcit/README.md" && fail "yuno_citizen: an empty Name in README.md"
build "$WORK/sktcit"

#
#   c_project, built with no warning
#
generate "$WORK" c_project "sktutil\n1.0.0\nUtility test\nArtGins\nsupport@artgins.com\nMIT\n"
grep -q "^MIT$" "$WORK/sktutil/README.md" || fail "c_project: the license is not in README.md"
build "$WORK/sktutil"

#
#   js_gclass and js_gclass_service, loaded by node against the gobj-js of
#   this tree (its src/, so no npm build is needed): each registers, the
#   child is created under the yuno and the service as a service, each
#   one's timer fires, nothing is logged as an error. Up to 7.28.6
#   js_gclass imported from "yunetas", a package that does not exist, and
#   could not load at all; js_gclass_service did not exist.
#
JSDIR="$WORK/js"
mkdir -p "$JSDIR/node_modules/@yuneta/gobj-js"
generate "$JSDIR" js_gclass "sktjs\nJS test\nArtGins\n"
generate "$JSDIR" js_gclass_service "sktjsvc\nJS service test\nArtGins\n"
if [ -f "$JSDIR/c_sktjs.js" ] && [ -f "$JSDIR/c_sktjsvc.js" ]; then
    grep -q 'const GCLASS_NAME = "C_SKTJS";' "$JSDIR/c_sktjs.js" || fail "js_gclass: GCLASS_NAME is not C_SKTJS"
    grep -q '^     \*  CHILD subscription model$' "$JSDIR/c_sktjs.js" || fail "js_gclass: no CHILD subscription block"
    grep -q '^     \*  SERVICE subscription model$' "$JSDIR/c_sktjsvc.js" || fail "js_gclass_service: no SERVICE subscription block"
    sed -i 's|trace_msg("Timeout");|trace_msg("Timeout service");|' "$JSDIR/c_sktjsvc.js"
    if command -v node > /dev/null 2>&1; then
        printf '{"type": "module", "exports": "./index.js"}\n' > "$JSDIR/node_modules/@yuneta/gobj-js/package.json"
        printf 'export * from "%s/kernel/js/gobj-js/src/index.js";\n' "$YUNETAS_BASE" \
            > "$JSDIR/node_modules/@yuneta/gobj-js/index.js"
        cat > "$JSDIR/run.mjs" <<'JS'
import {
    gobj_start_up, gobj_create_yuno, gobj_create, gobj_create_service,
    gobj_start, gobj_stop, gobj_destroy,
    register_c_yuno, register_c_timer, set_log_callback,
} from "@yuneta/gobj-js";
import {register_c_sktjs} from "./c_sktjs.js";
import {register_c_sktjsvc} from "./c_sktjsvc.js";

const logged = [];
gobj_start_up(null, null, null, null, null, null, null);
set_log_callback((level, msg) => {
    logged.push(`${level} ${msg}`);
});
register_c_yuno();
register_c_timer();
if(register_c_sktjs() < 0 || register_c_sktjsvc() < 0) {
    console.log("a register_c_*() failed");
    process.exit(1);
}
const yuno = gobj_create_yuno("yuno", "C_YUNO", {yuno_role: "sktjs"});
const child = gobj_create("sktjs", "C_SKTJS", {timeout: 50}, yuno);
const service = gobj_create_service("sktjsvc", "C_SKTJSVC", {timeout: 50}, yuno);
gobj_start(child);
gobj_start(service);
setTimeout(() => {
    for(const gobj of [child, service]) {
        gobj_stop(gobj);
        gobj_destroy(gobj);
    }
    console.log(logged.join("\n"));
    const fired = logged.includes("msg Timeout") && logged.includes("msg Timeout service");
    const errors = logged.filter(l => l.startsWith("error"));
    process.exit(fired && errors.length === 0 ? 0 : 1);
}, 500);
JS
        if ! (cd "$JSDIR" && node run.mjs) > "$WORK/run_js.log" 2>&1; then
            fail "a js gclass did not load, or its timer did not fire, or it logged an error"
            head -20 "$WORK/run_js.log"
        fi
    else
        echo "SKIP js_gclass run: no node in PATH"
    fi
else
    fail "js_gclass or js_gclass_service did not make c_sktjs.js / c_sktjsvc.js"
fi

#
#   project and project_commercial: each with a yuno_citizen made in its
#   yunos/ and added to yunos/CMakeLists.txt, built from the project root
#   with no warning; the LICENSE is the one of its kind, and the README
#   says the license asked for
#
for kind in project project_commercial; do
    P="$WORK/prj_$kind"
    mkdir -p "$P"
    generate "$P" $kind "sktapp\n\nApp test\nArtGins\nsupport@artgins.com\n\n"
    A="$P/sktapp"
    if [ ! -f "$A/yunos/CMakeLists.txt" ] || [ ! -f "$A/.gitignore" ]; then
        fail "$kind did not make sktapp/yunos/CMakeLists.txt and .gitignore"
        continue
    fi
    if [ $kind = project ]; then
        grep -q "^MIT License$" "$A/LICENSE" || fail "$kind: LICENSE is not MIT"
        grep -q "^MIT, see" "$A/README.md" || fail "$kind: the README does not say MIT"
    else
        grep -q "^This software is proprietary" "$A/LICENSE" || fail "$kind: LICENSE is not proprietary"
        grep -q "^Proprietary, see" "$A/README.md" || fail "$kind: the README does not say Proprietary"
        grep -q "THIRD-PARTY-NOTICES" "$A/README.md" || fail "$kind: the README does not say what a delivery carries"
    fi
    (cd "$A/yunos" && printf "sktappy\n\n1.0.0\nApp yuno\nArtGins\nsupport@artgins.com\nMIT\n" \
        | "$BIN" -p "$SKELETONS" yuno_citizen) > "$WORK/gen_${kind}_yuno.log" 2>&1 \
        || fail "yuno_citizen in $kind/yunos (see $WORK/gen_${kind}_yuno.log)"
    sed -i 's|^# add_subdirectory(my_yuno)$|add_subdirectory(sktappy)|' "$A/yunos/CMakeLists.txt"
    grep -q "^add_subdirectory(sktappy)$" "$A/yunos/CMakeLists.txt" || fail "$kind: the yuno was not added to yunos/"
    if build "$A"; then
        [ -x "$A/build/yunos/sktappy/sktappy" ] || fail "$kind: the yuno was not built"
    fi
done

#
#   No template holds a {{marker}} that nothing fills: yuno-skeleton says
#   each one (up to 7.28.6 it rendered {{yunoname}} empty, and said nothing)
#
if grep -h "has no value" "$WORK"/gen_*.log | grep -q .; then
    fail "a template holds a marker with no value:"
    grep -h "has no value" "$WORK"/gen_*.log | sort -u
fi

if [ $FAILURES -ne 0 ]; then
    echo "<-- TEST FAILED: yuno_skeleton/templates ($FAILURES)"
    exit 1
fi
echo "ok   yuno_skeleton/templates"
exit 0
