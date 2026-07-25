#!/bin/bash
#
#   Deploy the root nginx configuration for the Yuneta domains, plus the
#   conf.d/ drop-ins owned by this repo, to the serving node.
#
#   The node runs plain nginx at /yuneta/bin/nginx (NOT openresty: this node
#   needs no Lua auth). Other products push their own drop-ins into the same
#   conf.d/ and are left untouched by this script.
#
#   Usage:
#       ./deploy-conf.sh                # node = this directory's name
#       NODE=<host> ./deploy-conf.sh    # override the target host
#
set -euo pipefail

cd "$(dirname "$0")"

if [ -z "${NODE:-}" ]; then
    NODE=$(basename "$PWD")     # WARNING current directory must be name of URL
fi

NGINX_PREFIX=/yuneta/bin/nginx
CONF_DIR="$NGINX_PREFIX/nginx/conf"
SBIN="$NGINX_PREFIX/sbin/nginx"

#
#   Back up the live config
#
#   A fresh nginx install overwrites conf/nginx.conf with the stock default,
#   which silently drops every server block. Keep the outgoing copy so the
#   previous state is always recoverable on the node itself.
#
echo ">>> Backing up the live config on $NODE ..."
ssh "yuneta@$NODE" "
    mkdir -p '$CONF_DIR/conf.d'
    if [ -f '$CONF_DIR/nginx.conf' ]; then
        cp -a '$CONF_DIR/nginx.conf' '$CONF_DIR/nginx.conf.bak-\$(date +%Y%m%d-%H%M%S)'
    fi
"

#
#   Upload
#
echo ">>> Uploading nginx.conf and conf.d/ -> yuneta@$NODE:$CONF_DIR ..."
scp -q nginx.conf "yuneta@$NODE:$CONF_DIR/nginx.conf"
scp -q conf.d/*.conf "yuneta@$NODE:$CONF_DIR/conf.d/"

#
#   Validate, then reload
#
#   Validate before reloading so a bad conf can never take down the live
#   server; `nginx -t` failing aborts the script before the reload runs.
#
echo ">>> Validating ..."
ssh "yuneta@$NODE" "sudo $SBIN -t"

echo ">>> Reloading ..."
ssh "yuneta@$NODE" "sudo $SBIN -s reload"

echo ">>> Done. Root config live on $NODE."
