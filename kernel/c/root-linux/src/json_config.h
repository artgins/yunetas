/****************************************************************************
 *          JSON_CONFIG.H
 *
 *          Json configuration.
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

/*
 *  Dependencies
 */
#include <gobj.h>


#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Prototypes
 **************************************************/
/**rst**

.. function:: PUBLIC char *json_config(
        BOOL print_verbose_config,
        BOOL print_final_config,
        const char *fixed_config,
        const char *variable_config,
        const char *config_json_file,
        const char *parameter_config)

   Return a malloc'ed string with the final configuration
   of joining the json format input string parameters.

   If there is an error in json format, the function will exit
   printing the error.

   If ``print_verbose_config`` or ``print_final_config`` is TRUE then
   the function will print the result and will exit(0).

   The json string can contain one-line comments with combination: #^^

   The config is load in next order:

    #. fixed_config            string, this config is not writtable
    #. variable_config:        string, this config is writtable.
    #. use_config_file:        file of file's list, overwriting variable_config
    #. use_extra_config_file:  file of file's list, overwriting variable_config
    #. parameter_config:       string, overwriting variable_config

..warning::

  Remember **free** the returned string with jsonp_free().

  The json string can contain one-line comments with combination: ##^

  You can expand a dict of json data in a range, with {^^ ^^},  example::

        "{^^gossamers^^}": {
            "__range__": [12000,12002],
            "__vars__": {
                "__local_ip__": "192.168.1.24",
                "__remote_ip__": "192.168.1.4"
            },
            "__content__": {
                "PAOG_PID_1P_P(^^__range__^^)": {
                    "lHost": "(^^__local_ip__^^)",
                    "pid": "P(^^__range__^^)",
                    "kw_connex": {
                        "urls": [
                            "tcp://(^^__remote_ip__^^):102"
                        ]
                    }
                }
            }
        }

  will expand in::

        "gossamers": {
            "PAOG_PID_1P_P12000": {
                "lHost": "192.168.1.24",
                "pid": "P12000",
                "kw_connex": {
                    "urls": [
                        "tcp://192.168.1.4:102"
                    ]
                }
            },
            "PAOG_PID_1P_P12001": {
                "lHost": "192.168.1.24",
                "pid": "P12001",
                "kw_connex": {
                    "urls": [
                        "tcp://192.168.1.4:102"
                    ]
                }
            },
            "PAOG_PID_1P_P12002": {
                "lHost": "192.168.1.24",
                "pid": "P12002",
                "kw_connex": {
                    "urls": [
                        "tcp://192.168.1.4:102"
                    ]
                }
            }
        }


  A special key for json_config is ``__json_config_variables__``.
  If it exists then all strings inside a (^^ ^^) will be replaced
  by the value found in __json_config_variables__ dict.

  A ``__hostname__`` key is added to ``__json_config_variables__`` dict with the hostname.


**rst**/
PUBLIC char *json_config( /* **free** the returned string with jsonp_free() */
    BOOL print_verbose_config,     // WARNING if true will exit(0)
    BOOL print_final_config,       // WARNING if true will exit(0)
    const char *fixed_config,
    const char *variable_config,
    const char *config_json_file,
    const char *parameter_config,
    pe_flag_t quit
);

/**rst**

.. function:: PUBLIC char *helper_quote2doublequote(char *str)

   Change ' by ".
   Useful for easier json representation in C strings,
   **BUT** you cannot use true '

**rst**/
PUBLIC char *helper_quote2doublequote(char *str);

/**rst**

.. function:: PUBLIC char *helper_doublequote2quote(char *str)

   Change " by '.
   Useful for easier json representation in C strings,
   **BUT** you cannot use true "

**rst**/
PUBLIC char *helper_doublequote2quote(char *str);

#ifdef __cplusplus
}
#endif
