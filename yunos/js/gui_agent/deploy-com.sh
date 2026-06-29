#!/bin/sh
#
#   Deploy the built SPA to artgins.com, served per tenant/plane domain.
#
#   The SAME build is served at multiple domains; each derives its
#   control-center + BFF endpoints from its own hostname (src/conf/deploy.js):
#       artgins.yunetacontrol.com  -> agents  plane (CC 1996, BFF 1806)
#       artgins.yunetacontrol.ovh  -> agent22 plane (CC 1997, BFF 1807)
#
#   Usage:
#       npm run build
#       ./deploy-com.sh                              # artgins.yunetacontrol.com
#       ./deploy-com.sh artgins.yunetacontrol.ovh    # the .ovh plane
#
SSH_HOST="artgins.com"
DOMAIN="${1:-artgins.yunetacontrol.com}"

rsync -avzL --delete \
    --exclude \.webassets-cache --exclude \.sass-cache --exclude \.cache \
    ./dist/ \
    "yuneta@$SSH_HOST:/yuneta/gui/$DOMAIN"
