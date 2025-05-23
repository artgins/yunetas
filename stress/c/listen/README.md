# README

## stress-connections.js

stress connections with a javascript program using node.js

To install node.js:

    curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
    # Then reload shell:
    export NVM_DIR="$HOME/.nvm"
    . "$NVM_DIR/nvm.sh"

    # Install latest LTS version
    nvm install --lts

    # Set as default
    nvm use --lts
    nvm alias default lts/*

    cd /yuneta/development/yunetas/stress/c/listen
    npm install

## Install stress in agent

Move to stress directory:

    cd /yuneta/development/yunetas/stress/c/listen

and execute in agent:

    install-binary id=stress_listen content64=$$(stress_listen)
    create-config id=stress_listen.10 content64=$$(./stress_listen.10.json)
    create-yuno id=10 realm_id=utilities.artgins.com yuno_role=stress_listen yuno_name=10 must_play=1
