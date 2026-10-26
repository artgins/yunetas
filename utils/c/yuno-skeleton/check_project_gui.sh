#!/bin/sh
#
#   Check the gui/ of the project templates: generate a project with the
#   yuno-skeleton and the templates of this tree, install its gui and build
#   it (prebuild included: validate-locales and check-markup).
#
#   gobj-ui's deploy round runs it when it raises the template's ranges
#   (`skeletons/project_base/gui/package.json_tmpl`): a key the library adds
#   and the template's locales lack fails the prebuild of every app generated
#   from it (it happened to "{{name}}, {{n}}" with 7.26.35). It needs npm and
#   the registry, so ctest does not run it; yuno_skeleton/templates checks the
#   rest of the generated gui without a network.
#
#   Usage:
#       check_project_gui.sh
#
#          Copyright (c) 2026, ArtGins.
#          All Rights Reserved.
#
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/build/yuno-skeleton"
SKELETONS="$HERE/skeletons"

if [ ! -x "$BIN" ]; then
    echo "check_project_gui: yuno-skeleton not built: $BIN" >&2
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

#
#   Generate
#
cd "$TMP"
printf 'check\n0.1.0\nTemplate check\nArtGins\nsupport@artgins.com\nMIT\n' \
    | "$BIN" -p "$SKELETONS" project > generate.log 2>&1 || {
    cat generate.log >&2
    echo "check_project_gui: yuno-skeleton project failed" >&2
    exit 1
}
if grep -q "has no value" generate.log; then
    grep "has no value" generate.log >&2
    echo "check_project_gui: a marker with no value" >&2
    exit 1
fi

#
#   Install and build
#
cd "$TMP/check/gui"
npm install
npm run build
echo "check_project_gui: the project template generates a gui that builds"
