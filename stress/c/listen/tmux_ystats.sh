#!/bin/bash

SESSION_NAME="ystats"

# Start tmux session in detached mode
tmux new-session -d -s "$SESSION_NAME"

# Run ystats -o 5002 in the first (left) pane
tmux send-keys -t "$SESSION_NAME" 'ystats -o 5002' C-m

# Split the window horizontally (right pane)
tmux split-window -h -t "$SESSION_NAME"

# Run ystats -o 2002 in the second (right) pane
tmux send-keys -t "$SESSION_NAME".0.1 'ystats -o 2002' C-m

# Focus back on left pane (optional)
tmux select-pane -t "$SESSION_NAME".0.0

# Attach to session
tmux attach -t "$SESSION_NAME"

