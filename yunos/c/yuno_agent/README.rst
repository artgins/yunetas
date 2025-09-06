:orphan:

Yuno
=====

Name:
Role: yuneta_agent


Description:
------------

Yuneta Realm Agent

Example::

{
    "global": {
        "Agent.agent_environment":  {
            "daemon_log_handlers": {
                "to_file": {
                    "filename_mask": "W.log"
                }
            }
        }
    },
    "environment": {
        "node_owner": "artgins"
    },
    "__json_config_variables__": {
        "__input_url__": "ws://0.0.0.0:1991"
    }
}

License
-------

Licensed under the  `The MIT License <http://www.opensource.org/licenses/mit-license>`_.
See LICENSE.txt in the source distribution for details.
