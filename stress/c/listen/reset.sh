#!/bin/bash

# Check if the script is being sourced
(return 0 2>/dev/null)
if [[ $? -ne 0 ]]; then
    echo "⚠️  ERROR: You must source this script to apply ulimit to the current shell."
    echo "👉 Usage: source set_ulimit.sh"
    exit 1
fi

ulimit -n 200000
ycommand -c "kill-yuno id=2002 force=1"
ycommand -c "kill-yuno id=5002 force=1"
rm -rf /yuneta/store/queues/gate_msgs2/gate_energy^2002-0/
rm -rf /yuneta/store/yunovatiosdb/artgins/demo.yunovatios.es/tracks/
