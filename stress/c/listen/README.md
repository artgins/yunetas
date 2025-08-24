# README

## Help to debug

HELP to debug
To see the cpu and files opened 

    pidstat -p <pid> 1 -u -v -d -h

to see the systemcalls:

    sudo strace -p <pid>
    sudo strace -p <pid> -k     # to see the stack trace

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

# Test 27/May/2025: in ovh, 3 machines, C_TPC_S with legacy method 

Server  - stress_listen yuno, with config 11000 concurrent connections, legacy method,
Clients - 2 clients launching each one 5000 connections and disconnections in a loop and
in different machines.

Result:
    After of after, re-reaching the 10000 connections, many time lower than 10000.
    It seems good response (?) (TODO measure the time in the client side).
    CPU 100% permanent!

Advantages:
    No problem with the overload of connections, they are reject intermediately.
Cons:
    Perhaps more slower than the new method (1 accept event by TCP gobj), or not.
    Remember: legacy method is only one accept event for all TCP gobjs: own time to search one free.

JS1:
    /yuneta/development/yunetas/stress/c/listen/stress-connections.js -h 37.187.89.46 -c 5000 -d 20
JS2:
    /yuneta/development/yunetas/stress/c/listen/stress-connections.js -h 37.187.89.46 -c 15000 -d 30

Config:

        {
            'name': '__input_side__',
            'gclass': 'C_IOGATE',
            'autostart': false,
            'autoplay': false,
            'kw': {
            },
            'children': [
                {
                    'name': 'server_port',
                    'gclass': 'C_TCP_S',
                    'kw': {
                        'url': '(^^__input_url__^^)',
                        'backlog': 32767, #^^ new-method:32767, legacy: 32767
                        'use_dups': 0,
                        'child_tree_filter': {
                            'kw': {
                                '__gclass_name__': 'C_CHANNEL',
                                '__disabled__': false,
                                'connected': false
                            }
                        }
                    }
                }
            ],
            '[^^children^^]': {
                '__range__': [1,11000], #^^11000
                '__vars__': {
                },
                '__content__': {
                    'name': 'input-(^^__range__^^)',
                    'gclass': 'C_CHANNEL',
                    'children': [
                        {
                            'name': 'input-(^^__range__^^)',
                            'gclass': 'C_PROT_TCP4H',
                            'kw': {
                            },
                            'children': [
                                #^^
                                {
                                    'name': 'input-(^^__range__^^)',
                                    'gclass': 'C_TCP',
                                    'kw': {
                                    }
                                }
                            ]
                        }
                    ]
                }
            }
        }

# Test 27/May/2025: in ovh, 3 machines, C_TPC_S with new method

11000 maximum concurrent conections, atacking 2 clients with 5000 connections 
HERE we don't have protections against overload, i.e. more than 11000 connections, 
so the clients must wait timeout (2 minutes in my test) for receive a close of the socket by the kernel.

JS1:
/yuneta/development/yunetas/stress/c/listen/stress-connections.js -h 37.187.89.46 -c 5000 -d 20
JS2:
/yuneta/development/yunetas/stress/c/listen/stress-connections.js -h 37.187.89.46 -c 5000 -d 30

Result:
CPU with peaks of 60%, near all the time in 0% !!
Near all the time with the 10000 connections actives. 

Config:

                {
                    'name': 'server_port',
                    'gclass': 'C_TCP_S',
                    'kw': {
                        'url': '(^^__input_url__^^)',
                        'backlog': 32767, #^^ new-method:32767, legacy: 32767
                        'use_dups': 0
                    #^^    'child_tree_filter': {
                    #^^        'kw': {
                    #^^            '__gclass_name__': 'C_CHANNEL',
                    #^^            '__disabled__': false,
                    #^^            'connected': false
                    #^^        }
                    #^^    }
                    }
                }

# Test 30/May/2025: __input_side__ with new [^^children^^]

Fixed the slow start with thousands of children: 
legagy __range__ = [[1,11000]] very SLOW
Now you can use an integer

            '[^^children^^]': {                                     \n\
                '__range__': 11000, #^^11000                        \n\
                '__vars__': {                                       \n\
                },                                                  \n\


# Test 16/Jun/2025

    Two process with 5.000 connections and 50.000 msg/seg each one: 100.000 msg/seg
        ./stress-traffic.js -c 5000 -r 10
        ./stress-traffic.js -c 5000 -r 10

        cpu 100%

    With 90.000 msg/seg
        ./stress-traffic.js -c 5000 -r 9
        ./stress-traffic.js -c 5000 -r 9

        cpu 80%

    With 100.000 msg/seg and compiler CLANG and PROD compiling: 100.000 msg/seg
        ./stress-traffic.js -c 5000 -r 10
        ./stress-traffic.js -c 5000 -r 10

        cpu 87%

# Test 23/Jun/2025

    Compiled with sucessful the static binaries with musl

    Times of tests: CLang prod/test, Gcc prod/test, Musl prod/test

## Test results of clang_prod
```{include} ./test_results_clang_prod.md
```

## Test results of clang_debug
```{include} ./test_results_clang_debug.md
```

## Test results of gcc_prod
```{include} ./test_results_gcc_prod.md
```

## Test results of gcc_debug
```{include} ./test_results_gcc_debug.md
```

## Test results of musl_prod
```{include} ./test_results_musl_prod.md
```

## Test results of musl_debug
```{include} ./test_results_musl_debug.md
```
