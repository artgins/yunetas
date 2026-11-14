#!/bin/bash
#
#   Copyright (c) 2026, ArtGins.
#
#   Download the HiveMQ MQTT CLI (mqtt-cli.jar) into this directory, at the
#   pinned version, and check its sha256. Does nothing if it is already there.
#   run_hivemq_tests.sh calls it when the jar is missing.
#
#   Usage: get_mqtt_cli.sh [--force]      (--force: download it again)
#
set -e

#
#   Configuration
#
MQTT_CLI_VERSION=4.57.0
MQTT_CLI_SHA256=b642e3817c6cf23e86cf787f15e1a64f65e78a08bf48a4e4043c42c4ae38a223
MQTT_CLI_URL=https://github.com/hivemq/mqtt-cli/releases/download/v${MQTT_CLI_VERSION}/mqtt-cli-${MQTT_CLI_VERSION}.jar
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JAR="${SCRIPT_DIR}/mqtt-cli.jar"

if [[ -f "${JAR}" && "$1" != "--force" ]]; then
    echo "mqtt-cli.jar already in ${SCRIPT_DIR}"
    exit 0
fi

#
#   Download and check
#
echo "Downloading ${MQTT_CLI_URL}"
curl -fL --retry 3 -o "${JAR}.tmp" "${MQTT_CLI_URL}"
SUM=$(sha256sum "${JAR}.tmp" | cut -d' ' -f1)
if [[ "${SUM}" != "${MQTT_CLI_SHA256}" ]]; then
    rm -f "${JAR}.tmp"
    echo "ERROR: sha256 of mqtt-cli-${MQTT_CLI_VERSION}.jar is ${SUM}, expected ${MQTT_CLI_SHA256}" >&2
    exit 1
fi
mv "${JAR}.tmp" "${JAR}"
echo "mqtt-cli ${MQTT_CLI_VERSION} installed: ${JAR}"
