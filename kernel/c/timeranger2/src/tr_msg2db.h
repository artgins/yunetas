/****************************************************************************
 *          tr_msg2db.h
 *
 *          Messages (ordered by key (id),pkey2) with TimeRanger
 *
 *          Double dict of messages
 *          Load in memory a iter of topic's messages ordered by a sub-key
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.

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

    The load is forward and keeps, per id and pkey2, the LAST message
    loaded. A key (id) whose history did not load whole (a md2 file of it
    cannot be read) is loaded again BACKWARD, newest first, keeping the
    FIRST message of each pkey2: what is served of it is exactly its
    current message, for every pkey2 whose newest message is newer than
    the damage. A pkey2 whose newest message was not read is ABSENT (an
    older one may be read, and it is not served as current). The id is
    marked incomplete (msg2db_id_incomplete()) and an ERROR names it
    ("msg2db: a key whose history did not load whole: only the messages
    newer than the damage are served...").

    The next message of the id goes to the file of the current period (it
    is appended with the time of now). When the damaged file is an OLDER
    file, the next message of a pkey2 is stored and served as it arrives.
    When the damaged file IS the file of the current period, tranger
    refuses every append into it ("Cannot append record, its file is
    flagged unreadable"): NO new message of the id, of any pkey2, is
    stored or served until the file is repaired or the period changes. A
    second ERROR says so at the open ("msg2db: the damaged file of the key
    is the file of the current period..."). The alarms of db_history in the
    projects use a "%Y" tranger, one file per key and YEAR: every new alarm
    message of that device is refused until the file is repaired or the
    year changes. This is for REAL damage only: a md2 that cannot be
    opened or read. A md2 whose last row is torn (its size is not a whole
    number of rows: a power cut during the write of a row) is not damage.
    It is an append that was never acknowledged, and a master tranger cuts
    it back at the open with a WARNING ("md2 file of the key ends in a
    part of a row: an append that was never acknowledged was cut back"):
    the id loads whole and its next message is stored and served.
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

/**rst**
    TRUE when the history of `id` did not load whole at the open: a
    msg2db_get_message() of it that answers NULL means UNKNOWN (its newest
    message was not read), not "there is none". The messages it answers
    are current. It stays TRUE while the msg2db is open, whatever messages
    arrive: the damaged file is still on disk, and other pkey2 of the id
    may still be unknown.
**rst**/
PUBLIC BOOL msg2db_id_incomplete(
    json_t *tranger,
    const char *msg2db_name,
    const char *topic_name,
    const char *id
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
