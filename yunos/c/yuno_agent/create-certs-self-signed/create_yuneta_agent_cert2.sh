#!/bin/bash
#
#   The agent's pair is made on each node by yuneta-agent-cert (7.30.6+):
#   up to 7.30.5 it was made once with this script and shipped, the same key
#   on every node. Run that one instead:
#
#       /usr/libexec/yuneta/yuneta-agent-cert ensure     (a packaged node, as root)
#       packages/templates/yuneta-agent-cert ensure      (from the source)
#
exec "$(dirname "$0")/../../../../packages/templates/yuneta-agent-cert" "${@:-ensure}"
