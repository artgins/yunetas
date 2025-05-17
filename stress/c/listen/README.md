# README

- listen: stress listen connections with a javascript program

## Install in agent

Move to stress directory:

    - cd /yuneta/development/yunetas/stress/c/listen

and execute in agent:

    - install-binary id=stress_listen content64=$$(stress_listen)
    - create-config id=stress_listen.10 content64=$$(./stress_listen.10.json)
    - create-yuno id=10 realm_id=utilities.artgins.com yuno_role=stress_listen yuno_name=10 must_play=1
