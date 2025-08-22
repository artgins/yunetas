#!/bin/sh

#
#   Copy, from the output of DEVELOPMENT realm (/yuneta/development/output), the next items:
#       binaries:
#               - yuneta_agent and his conf to /yuneta/agent/ directory.
#               - yuneta (cli) and ybatch (batch) to /yuneta/bin/ directory.
#
#       scripts needed for make service:
#               - service directory
#
#
#   Yuneta: REALM of DEPLOY
#   =======================
#
#   A yuneta node can be deploy in any node (virtual machine, dedicated machine, container, etc),
#   with the next requirements.
#
#   Requirements
#   ------------
#
#   Python versión 2.7 or upper
#   Linux kernel 3.9 or upper (Yuneta use SO_REUSEPORT socket option)
#   gcc 4.8 o superior
#
#   S.O. world: Actions as *root* user
#   ----------------------------------
#
#   Create a file-system and user for yuneta environment.
#
#   File system: /yuneta (>10G)
#   User: yuneta
#   Group: gyuneta
#
#   Configuration of /yuneta directory:
#
#       chown yuneta:yuneta /yuneta
#       chmod 775 /yuneta
#       chmod g+s /yuneta
#
#
#   S.O. world: Actions as *yuneta* user
#   ------------------------------------
#
#   (IMPORTANT execute this as yuneta user!)
#
#       1- Deploy the Yuneta basic run-time in /yuneta.
#           $ tar xzf yuneta-run-time-version.tar.gz
#
#       2- Execute the next commands, as root:
#               1.- Install the yuneta agent service in the node with the command:
#                       $ sudo /yuneta/agent/service/install-yuneta-service.sh
#
#               2.- Start the service (as you want).
#                       $ sudo service yuneta_agent start
#
#       3- Give the password of *yuneta* user to Yuneta world people.
#
#   These are the files supplied in the basic yuneta run-time.
#       /
#       │
#       └──yuneta/
#           ├── agent
#           │   ├── ncurses/                        (data files of ncurses 6.0)
#           │   ├── service
#           │   │   ├── install-yuneta-service.sh   (script to install as service the Yuneta Agent)
#           │   │   ├── remove-yuneta-service.sh    (script to remove as service the Yuneta Agent)
#           │   │   └── yuneta_agent                (script for services, to put in /etc/init.d/)
#           │   ├── yuneta_agent                    (binary of Agent yuno)
#           │   └── yuneta_agent.json               (config of Agent yuno)
#           └── bin
#               ├── yuneta                          (binary of Yuneta CLI)
#               ├── ylist                           (binary to list yunos)
#               ├── ystats                          (binary to stats yunos)
#               ├── ytestconfig                     (binary to test json configurations)
#               ├── ybatch                          (binary of Yuneta Batch)
#               ├── ycommand                        (binary of Yuneta command)
#               └── yshutdown                       (binary to kill all yunos and/or yuneta_agent)
#
#   All relative to agent yuno will be in the /yuneta/agent/ directory.
#   All other utilities and yunos must be in the /yuneta/bin/ directory.
#
#   Yuneta world: as *yuneta* user
#   ------------------------------
#
#   Once yuneta_agent service is running, and the partner hosts are accesible,
#   nothing more is needed from other worlds.
#
#   Deploy/Operation/Control of Realms/Yunos belongs to the Yuneta world,
#   a world of distributed and collaborative applications.
#
#   To manage Realms and Yunos in Yuneta world you need:
#       - Install the Yuneta development kit or the agent run-time.
#       - Training in Yuneta philosophy and tools.
#
#   All operations are done with the tools supplied by Yuneta:
#       - yuneta (CLI).
#       - ybatch (Batch).
#       - optional GUI (web application).
#
#   The deploy/operation of yunos can be done in two modes:
#       1.- Manual, with yuneta, the CLI to work in consoles.
#           You can operate any remote accessible node from your site.
#           But you must type writing your commands and wait for their responses.
#
#       2.- Semi-automatic, with ybatch, to work in console too.
#           Here you can execute a list of commands previously written in a json format file.
#           And can be executed once time or forever, in an infinite loop.
#           Build all scripts you need to automate your work.
#
#   HACK Remember set /yuneta/bin/ in the environment PATH variable.
#   For example, add in $HOME/.bashrc the next lines:
#       if [ -d "$HOME/bin" ] ; then
#           PATH="$HOME/bin:$PATH"
#       fi
#
#       if [ -d "/yuneta/bin" ] ; then
#           PATH="/yuneta/bin:$PATH"
#       fi
#
#

#  Exit immediately if a command exits with a non-zero status.
set -e

BINARY_AGENT="./yuneta_agent"
CONFIG_AGENT="./yuneta_agent.json"
cp -v $BINARY_AGENT /yuneta/agent/
cp -v -n $CONFIG_AGENT /yuneta/agent/

SERVICE_SCRIPTS=/yuneta/development/output/agent/service/
cp -v -a $SERVICE_SCRIPTS /yuneta/agent/

mkdir -p /yuneta/agent/certs
CERTS_SCRIPTS=/yuneta/development/output/agent/certs/*
cp -v -n -a $CERTS_SCRIPTS /yuneta/agent/certs
