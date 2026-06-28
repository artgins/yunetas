#!/bin/sh
#
#   Deploy the built SPA to artgins.com, served as agents.yunetacontrol.com.
#
#   The served directory name differs from the SSH host: the files live on
#   artgins.com (the company server) under /yuneta/gui/agents.yunetacontrol.com,
#   which the nginx vhost serves for server_name agents.yunetacontrol.com.
#
#   Run `npm run build` first (or rely on prebuild), then ./deploy-com.sh
#
SSH_HOST="artgins.com"
GUI_DIR="agents.yunetacontrol.com"

rsync -avzL --delete \
    --exclude \.webassets-cache --exclude \.sass-cache --exclude \.cache \
    ./dist/ \
    "yuneta@$SSH_HOST:/yuneta/gui/$GUI_DIR"
