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

## Measures Connections

Performance 22-May-2025 in my machine with 10.000
#TIME (count: 10000): elapsed 0.352265134 s, ops/sec 28387.71 -> OPENED 10000

Performance 22-May-2025 in my machine with 30.000
#TIME (count: 10000): elapsed 0.75 s, ops/sec 13314 -> OPENED 10000
#TIME (count: 20000): elapsed 1.35 s, ops/sec 14726 -> OPENED 20000
#TIME (count: 30000): elapsed 2.45 s, ops/sec 12235 -> OPENED 30000

## Measures Received messages (no jobs, only decoded the received message and publish, no disk)

Servidor OVH, 1 core:
    50000 msg/sec, 76% CPU

My laptop, 1 core:
    80000 msg/sec, 70% CPU
    100000 msg/sec, 95% CPU

## Measures Memory
    10 concurrent connecns internal 439558 bytes, total (Gnome system monitor) 786.4 Kb
    1000 concurrent: internal     24011218 bytes, total (Gnome system monitor) 32.2 MB
    10000 concurrent    internal 238456984 0.222 GB bytes, total (Gnome system monitor) 318.7 MB

    11000 concurrent    internal 262262984 0.2443 GB GB bytes, total (Gnome system monitor) 350.1 MB, 10 sec

    30000 concurrent    internal 714839128 0.6657 GB bytes, total (Gnome system monitor) 954.7 MB, 1 minute
    50000 concurrent: internal  1191483416 1.109 GB bytes, total (Gnome system monitor) 1.6 GB, 3 minutes
    100000 concurrent: internal 2382831998 2.219 GB bytes, total (Gnome system monitor) 3.2 GB, 12 minutes 
    1310000 concurrent: internal 2.9 GB, total (Gnome system monitor) 4.2 GB
    1500000 concurrent: internal 3575529150 3.33 GB, total (Gnome system monitor) 4.8 GB, 24 minutos
