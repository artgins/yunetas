#!/bin/bash
#
#   Run the controlcenter by hand, from its build dir, with the three config
#   files the agent wrote for one of its instances.
#
#   CCENTER_BIN     the bin/ dir of that instance:
#                   /yuneta/realms/<owner>/<realm>/controlcenter^<id>/bin
#   CCENTER_NAME    the yuno name in its config files:
#                   <n>-controlcenter^<name>.json
#
#   Usage:
#       CCENTER_BIN=<dir> CCENTER_NAME=<name> ./ccenter.sh [controlcenter args]
#

#
#   Check the parameters
#
if [ -z "${CCENTER_BIN}" ] || [ -z "${CCENTER_NAME}" ]; then
    echo "Usage: CCENTER_BIN=<dir> CCENTER_NAME=<name> $0 [controlcenter args]" >&2
    exit 1
fi

#
#   Run
#
C1="${CCENTER_BIN}/1-controlcenter^${CCENTER_NAME}.json"
C2="${CCENTER_BIN}/2-controlcenter^${CCENTER_NAME}.json"
C3="${CCENTER_BIN}/3-controlcenter^${CCENTER_NAME}.json"

./controlcenter --config-file="[\"${C1}\",\"${C2}\",\"${C3}\"]" "$@"
