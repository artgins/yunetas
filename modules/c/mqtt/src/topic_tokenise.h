/****************************************************************************
 *          topic_tokenise.h
 *          MQTT Subscription Topic Tokenizer
 *
 *  Header file for MQTT subscription topic parsing utilities.
 *  Provides functions to tokenize MQTT topics into hierarchical levels
 *  with support for regular topics, system topics ($SYS), and
 *  shared subscriptions ($share).
 *
 *  Port of the Mosquitto MQTT broker implementation.
 *
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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
 *                   Can be NULL if caller doesn't need the share name.
 *
 *  Returns:
 *      MOSQ_ERR_SUCCESS  - Success
 *      MOSQ_ERR_INVAL    - Invalid (empty) topic
 *      MOSQ_ERR_NOMEM    - Memory allocation failed
 *      MOSQ_ERR_PROTOCOL - Invalid shared subscription format
 *
 *  Output Examples:
 *  +-----------------------------+------------------------------------+-----------+
 *  | Input subtopic              | topics[] array                     | sharename |
 *  +-----------------------------+------------------------------------+-----------+
 *  | "sport/tennis"              | ["", "sport", "tennis", NULL]      | NULL      |
 *  | "sport/tennis/#"            | ["", "sport", "tennis", "#", NULL] | NULL      |
 *  | "$SYS/broker/load"          | ["$SYS", "broker", "load", NULL]   | NULL      |
 *  | "$share/group1/sport/tennis"| ["", "sport", "tennis", NULL]      | "group1"  |
 *  +-----------------------------+------------------------------------+-----------+
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
 *
 *  Example Usage:
 *      char *local_sub = NULL;
 *      char **topics = NULL;
 *      const char *sharename = NULL;
 *      int rc;
 *
 *      rc = sub__topic_tokenise("sport/tennis/#", &local_sub, &topics, &sharename);
 *      if(rc == MOSQ_ERR_SUCCESS) {
 *          // Use topics array...
 *          // topics[0] = ""
 *          // topics[1] = "sport"
 *          // topics[2] = "tennis"
 *          // topics[3] = "#"
 *          // topics[4] = NULL
 *
 *          // Cleanup
 *          gbmem_free(local_sub);
 *          gbmem_free(topics);
 *      }
 ***************************************************************************/
int sub__topic_tokenise(
    const char *subtopic,
    char **local_sub,       // remember free with gbmem_free()
    char ***topics,         // remember free with gbmem_free()
    const char **sharename
);


#ifdef __cplusplus
}
#endif
