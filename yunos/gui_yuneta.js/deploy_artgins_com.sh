#!/bin/sh
rsync -avzL --delete \
    --exclude \.webassets-cache --exclude \.sass-cache --exclude \.cache \
    ./tags/0.00.aa/ \
    yuneta@artgins.com:/yuneta/gui/artgins.yunetacontrol.com
