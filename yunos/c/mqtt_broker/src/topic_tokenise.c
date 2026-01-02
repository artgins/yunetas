/****************************************************************************
 *          topic_tokenise.c
 *          MQTT Subscription Topic Tokenizer
 *
 *  Implementation of MQTT subscription topic parsing utilities.
 *  Provides functions to tokenize MQTT topics into hierarchical levels
 *  with support for regular topics, system topics ($SYS), and
 *  shared subscriptions ($share).
 *
 *  Port of the Mosquitto MQTT broker implementation.

    Copyright (c) 2010-2019 Roger Light <roger@atchoo.org>

    All rights reserved. This program and the accompanying materials
    are made available under the terms of the Eclipse Public License 2.0
    and Eclipse Distribution License v1.0 which accompany this distribution.

    The Eclipse Public License is available at
       https://www.eclipse.org/legal/epl-2.0/
    and the Eclipse Distribution License is available at
      http://www.eclipse.org/org/documents/edl-v10.php.

    SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

    Contributors:
       Roger Light - initial implementation and documentation.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <gobj.h>
#include "topic_tokenise.h"

/***************************************************************************
 *  strtok_hier - Hierarchical Topic Tokenizer
 *
 *  Custom strtok-like function that splits strings by '/' (MQTT topic separator).
 *  Unlike standard strtok, this is designed specifically for MQTT topic hierarchy.
 *
 *  Parameters:
 *      str      - String to tokenize (pass NULL after first call)
 *      saveptr  - Pointer to maintain state between calls
 *
 *  Returns:
 *      Pointer to next token, or NULL when exhausted
 *
 *  Example usage:
 *      char *saveptr = NULL;
 *      char topic[] = "sport/tennis/player";
 *
 *      token = strtok_hier(topic, &saveptr);   // Returns "sport"
 *      token = strtok_hier(NULL, &saveptr);    // Returns "tennis"
 *      token = strtok_hier(NULL, &saveptr);    // Returns "player"
 *      token = strtok_hier(NULL, &saveptr);    // Returns NULL
 *
 *  Note: Modifies the original string by inserting '\0' at '/' positions
 ***************************************************************************/
static char *strtok_hier(char *str, char **saveptr)
{
    char *c;

    /*
     *  First call: initialize saveptr with the input string
     *  Subsequent calls: str is NULL, we continue from saveptr
     */
    if(str != NULL) {
        *saveptr = str;
    }

    /*
     *  No more data to process
     */
    if(*saveptr == NULL) {
        return NULL;
    }

    /*
     *  Search for the next '/' separator
     */
    c = strchr(*saveptr, '/');

    if(c) {
        /*
         *  Found a separator:
         *  - Save current position as the token to return
         *  - Advance saveptr past the '/'
         *  - Null-terminate current token by replacing '/' with '\0'
         *
         *  Before: saveptr -> "sport/tennis/player"
         *  After:  saveptr -> "tennis/player", returns "sport\0"
         */
        str = *saveptr;
        *saveptr = c + 1;
        c[0] = '\0';
    } else if(*saveptr) {
        /*
         *  No separator found, but we have remaining string:
         *  - Return the final segment
         *  - Set saveptr to NULL (signals completion on next call)
         *
         *  Before: saveptr -> "player"
         *  After:  saveptr = NULL, returns "player"
         */
        str = *saveptr;
        *saveptr = NULL;
    }

    return str;
}

/***************************************************************************
 *  sub__topic_tokenise - MQTT Subscription Topic Tokenizer
 *
 *  Parses an MQTT subscription topic into an array of topic levels.
 *  Handles regular topics, system topics ($SYS), and shared subscriptions ($share).
 *
 *  Parameters:
 *      subtopic   - Input: subscription topic string (e.g., "sport/tennis/#")
 *      local_sub  - Output: duplicated string (caller must free)
 *      topics     - Output: NULL-terminated array of topic level pointers
 *      sharename  - Output: shared subscription group name (NULL if not shared)
 *
 *  Returns:
 *      MOSQ_ERR_SUCCESS  - Success
 *      MOSQ_ERR_INVAL    - Invalid (empty) topic
 *      MOSQ_ERR_NOMEM    - Memory allocation failed
 *      MOSQ_ERR_PROTOCOL - Invalid shared subscription format
 *
 *  Output Examples:
 *  +-----------------------------+--------------------------------+-----------+
 *  | Input subtopic              | topics[] array                 | sharename |
 *  +-----------------------------+--------------------------------+-----------+
 *  | "sport/tennis"              | ["", "sport", "tennis", NULL]  | NULL      |
 *  | "sport/tennis/#"            | ["", "sport", "tennis", "#",   | NULL      |
 *  |                             |  NULL]                         |           |
 *  | "$SYS/broker/load"          | ["$SYS", "broker", "load",     | NULL      |
 *  |                             |  NULL]                         |           |
 *  | "$share/group1/sport/tennis"| ["", "sport", "tennis", NULL]  | "group1"  |
 *  +-----------------------------+--------------------------------+-----------+
 *
 *  Key Design Notes:
 *  - Regular topics get an empty string "" prefix for uniform tree traversal
 *  - System topics ($SYS, etc.) do NOT get the prefix (start with '$')
 *  - Shared subscriptions ($share/name/topic) extract the group name and
 *    return the actual topic with the "" prefix
 *
 *  Memory Management:
 *  - Caller must free both *local_sub and *topics on success
 *  - topics[] pointers reference memory inside local_sub (don't free individually)
 ***************************************************************************/
int sub__topic_tokenise(
    const char *subtopic,
    char **local_sub,
    char ***topics,
    const char **sharename
) {
    char *saveptr = NULL;
    char *token;
    int count;
    int topic_index = 0;
    int i;
    size_t len;

    /*----------------------------------------------------------------------*
     *  STEP 1: Validate input
     *          Empty topics are not allowed in MQTT
     *----------------------------------------------------------------------*/
    len = strlen(subtopic);
    if(len == 0) {
        return -1;
    }

    /*----------------------------------------------------------------------*
     *  STEP 2: Duplicate the input string
     *          We need a working copy because strtok_hier modifies it
     *          in-place by inserting '\0' characters at '/' positions
     *----------------------------------------------------------------------*/
    *local_sub = gbmem_strdup(subtopic);
    if((*local_sub) == NULL) {
        return -1;
    }

    /*----------------------------------------------------------------------*
     *  STEP 3: Count topic levels
     *          Count '/' separators to determine array size needed
     *
     *          Example: "sport/tennis/player"
     *                         ^      ^
     *                         |      +-- 2nd '/'
     *                         +-- 1st '/'
     *                   count = 3 (two separators + 1)
     *----------------------------------------------------------------------*/
    count = 0;
    saveptr = *local_sub;
    while(saveptr) {
        saveptr = strchr(&saveptr[1], '/');  /* Start search from position 1 */
        count++;
    }

    /*----------------------------------------------------------------------*
     *  STEP 4: Allocate topics array
     *          Size = count + 3 to accommodate:
     *            - Potential empty string prefix for regular topics
     *            - Potential $share and sharename slots
     *            - NULL terminator
     *----------------------------------------------------------------------*/
    *topics = gbmem_calloc((size_t)(count + 3), sizeof(char *));
    if((*topics) == NULL) {
        gbmem_free(*local_sub);
        return -1;
    }

    /*----------------------------------------------------------------------*
     *  STEP 5: Add empty string prefix for regular topics
     *
     *          Regular topics (not starting with '$') get an empty string
     *          prepended. This normalizes the array structure so that
     *          subscription tree traversal works uniformly.
     *
     *          Why? The subscription tree root has children for:
     *            - "" (empty) -> regular topics branch
     *            - "$SYS"     -> system topics branch
     *            - "$share"   -> shared subscriptions (processed later)
     *
     *          Example results:
     *            "sport/tennis"    -> ["", "sport", "tennis", NULL]
     *            "$SYS/broker"     -> ["$SYS", "broker", NULL]
     *----------------------------------------------------------------------*/
    if((*local_sub)[0] != '$') {
        (*topics)[topic_index] = "";
        topic_index++;
    }

    /*----------------------------------------------------------------------*
     *  STEP 6: Tokenize the topic string
     *          Split by '/' and store pointers in the topics array
     *
     *          After this loop for "sport/tennis/player":
     *            topics[0] = ""        (from step 5)
     *            topics[1] = "sport"
     *            topics[2] = "tennis"
     *            topics[3] = "player"
     *            topics[4] = NULL      (from calloc initialization)
     *----------------------------------------------------------------------*/
    token = strtok_hier((*local_sub), &saveptr);
    while(token) {
        (*topics)[topic_index] = token;
        topic_index++;
        token = strtok_hier(NULL, &saveptr);
    }

    /*----------------------------------------------------------------------*
     *  STEP 7: Handle shared subscriptions ($share/groupname/topic/...)
     *
     *          MQTT 5.0 shared subscriptions format:
     *            $share/{ShareName}/{TopicFilter}
     *
     *          Example: "$share/consumer-group/sport/tennis/#"
     *            - ShareName: "consumer-group"
     *            - TopicFilter: "sport/tennis/#"
     *
     *          Processing:
     *            1. Validate format (minimum 3 levels, non-empty topic)
     *            2. Extract sharename
     *            3. Remove $share and sharename from array
     *            4. Add empty prefix (treat as regular topic)
     *
     *          Before: ["$share", "consumer-group", "sport", "tennis", "#"]
     *          After:  ["", "sport", "tennis", "#", NULL]
     *                  sharename = "consumer-group"
     *----------------------------------------------------------------------*/
    if(!strcmp((*topics)[0], "$share")) {
        /*
         *  Validate shared subscription format:
         *  - Must have at least 3 parts: $share, sharename, topic
         *  - Topic filter cannot be empty (count==3 with empty [2])
         */
        if(count < 3 || (count == 3 && strlen((*topics)[2]) == 0)) {
            gbmem_free(*local_sub);
            gbmem_free(*topics);
            return -1;
        }

        /*
         *  Extract the share group name if caller wants it
         */
        if(sharename) {
            (*sharename) = (*topics)[1];
        }

        /*
         *  Shift array to remove $share and sharename:
         *
         *  Before: [0]="$share" [1]="group" [2]="sport" [3]="tennis" [4]=NULL
         *
         *  Shift loop (i from 1 to count-2):
         *    i=1: [1] = [2] -> [1]="sport"
         *    i=2: [2] = [3] -> [2]="tennis"
         *    i=3: [3] = [4] -> [3]=NULL
         *
         *  After:  [0]="$share" [1]="sport" [2]="tennis" [3]=NULL
         *
         *  Then fix [0] and ensure NULL termination:
         *  Final:  [0]="" [1]="sport" [2]="tennis" [3]=NULL
         */
        for(i = 1; i < count - 1; i++) {
            (*topics)[i] = (*topics)[i + 1];
        }
        (*topics)[0] = "";
        (*topics)[count - 1] = NULL;
    }

    return 0;
}
