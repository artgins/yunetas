:orphan:

Yuno
=====

Name:
Role: yuneta_agent


Description:
------------

Yuneta Realm Agent

{
    "global": {
        "Agent.startup_command":    "Command to execute at startup",
        "Agent.agent_environment":  "Agent environment. Override the yuno environment.",
        "Agent.node_variables":     "Global to Node json config variables."
    }
}

Example::

{
    "global": {
        "Agent.startup_command": "/yuneta/bin/nginx/sbin/nginx",
        "Agent.agent_environment":  {
            "daemon_log_handlers": {
                "to_file": {
                    "filename_mask": "W.log"
                }
            }
        }
    },
    "__json_config_variables__": {
        "__input_url__": "ws://0.0.0.0:1991",
        "__input_host__": "0.0.0.0",
        "__input_port__": "1991"
    }
}

License
-------

Licensed under the  `The MIT License <http://www.opensource.org/licenses/mit-license>`_.
See LICENSE.txt in the source distribution for details.
