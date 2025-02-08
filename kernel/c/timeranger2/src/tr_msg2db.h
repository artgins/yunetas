/****************************************************************************
 *          TR_MSG2.C
 *
 *          Messages (ordered by key (id),pkey2) with TimeRanger
 *
 *          Double dict of messages
 *          Load in memory a iter of topic's messages ordered by a sub-key
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024, ArtGins.

 *          All Rights Reserved.
 *
 ****************************************************************************/
#pragma once

#include "timeranger2.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Desc
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
/**rst**

    Rules:
        - Keys (id's) can't contain ` nor ^ characters.
        - A fkey field only can be fkey once, i.e, only can have one parent

**rst**/

/**rst**
    Open a message2 db (Remember previously open tranger2_startup())

    List of pkey/pkey2 of nodes of data.
    Node's data contains attributes, in json (record, node, message, ... what you want)


    HACK Conventions:
        1) the pkey of all topics must be "id".
        2) the "id" field (primary key) MUST be a string.
        3) define a second index `pkey2`, MUST be a string

    Option "persistent"
        Try to load the schema from file
        File has precedence.
        Once saved,
            if you want to change the schema
            then you must change the schema version and topic_version
**rst**/

PUBLIC json_t *msg2db_open_db(
    json_t *tranger,
    const char *msg2db_name,
    json_t *jn_schema,  // owned
    const char *options // "persistent"
);

PUBLIC int msg2db_close_db(
    json_t *tranger,
    const char *msg2db_name
);

PUBLIC json_t *msg2db_append_message( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *kw,    // owned
    const char *options // "permissive"
);

PUBLIC json_t *msg2db_list_messages( // Return MUST be decref
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    json_t *jn_ids,     // owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // not owned
        json_t *jn_filter   // owned
    )
);

PUBLIC json_t *msg2db_get_message( // Return is NOT YOURS
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id,
    const char *id2
);

/*
 *  Utilities
 */
PUBLIC char *build_msg2db_index_path(
    char *bf,
    int bfsize,
    const char *msg2db_name,
    const char *topic_name,
    const char *key
);

#ifdef __cplusplus
}
#endif
