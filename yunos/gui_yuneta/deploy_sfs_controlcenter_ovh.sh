#!/bin/sh
rsync -avzL --delete \
    --exclude \.webassets-cache --exclude \.sass-cache --exclude \.cache \
    ./tags/0.00.aa/ \
    yuneta@yuneta2:/yuneta/gui/sfs.yunetacontrol.ovh
