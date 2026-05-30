yshutdown
mv /yuneta/store/agent/yuneta_agent.trdb/ /yuneta/store/agent/yuneta_agent.trdb-`date +%Y-%m-%d-T%H-%M-%S`
rm -rf /yuneta/store/agent/yuneta_agent.trdb/

/yuneta/agent/yuneta_agent --start --config-file=/yuneta/agent/yuneta_agent.json

ycommand -c "create-realm id=utilities.artgins.com realm_owner=artgins realm_role=artgins realm_name=utilities realm_env=com"

if [ -d /yuneta/development/yunetas ]; then
    cd /yuneta/development/yunetas/stress/c/listen/deploy-yuno || exit 1
elif [ -d "$HOME/yunetaprojects/yunetas" ]; then
    cd "$HOME/yunetaprojects/yunetas/stress/c/listen/deploy-yuno" || exit 1
fi

bash ./_deploy_all.sh

# Launch then play in two steps so each command returns a single response
# (run-yuno play=0 only launches; play-yuno starts the services).
ycommand -c "run-yuno play=0"
ycommand -c "play-yuno"
