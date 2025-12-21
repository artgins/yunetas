/****************************************************************************
 *          topic_tokenise.h
 *
 *          Topic tokenization functions using Yunetas split helpers
 *
 *          Rewrite of Mosquitto's sub__topic_tokenise() using Yunetas
 *          split* functions from helpers.h
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************
 *  Function: sub__topic_tokenise
 *
 *  Tokenise a topic or subscription string into an array of strings
 *  representing the topic hierarchy.
 *
 *  For example:
 *      subtopic: "a/deep/topic/hierarchy"
 *      Would result in:
 *          topics[0] = "a"
 *          topics[1] = "deep"
 *          topics[2] = "topic"
 *          topics[3] = "hierarchy"
 *          topics[4] = NULL (terminator)
 *
 *      subtopic: "/a/deep/topic/hierarchy/"
 *      Would result in:
 *          topics[0] = "" (empty segment)
 *          topics[1] = "a"
 *          topics[2] = "deep"
 *          topics[3] = "topic"
 *          topics[4] = "hierarchy"
 *          topics[5] = "" (empty segment)
 *          topics[6] = NULL (terminator)
 *
 *  Note: Empty segments are stored as "" (empty strings), not NULL.
 *        NULL is used only as the array terminator.
 *
 *  Parameters:
 *      subtopic - the subscription/topic to tokenise
 *      topics   - a pointer to store the array of strings (NULL-terminated)
 *      count    - an int pointer to store the number of items
 *
 *  Returns:
 *      0  - on success
 *      -1 - on error (invalid parameters or memory allocation failed)
 ***************************************************************************/
int sub__topic_tokenise(
    const char *subtopic,
    char ***topics,
    int *count
);

/***************************************************************************
 *  Function: sub__topic_tokens_free
 *
 *  Free memory that was allocated in sub__topic_tokenise
 *
 *  The topics array is NULL-terminated. Empty segments are stored as ""
 *  (empty strings), not NULL.
 *
 *  Parameters:
 *      topics - pointer to string array (NULL-terminated)
 *
 *  Returns:
 *      0  - on success
 *      -1 - on error (invalid parameters)
 ***************************************************************************/
int sub__topic_tokens_free(
    char ***topics
);

/***************************************************************************
 *  Function: sub__topic_tokenise_v2
 *
 *  Extended version that handles MQTT v5 shared subscriptions.
 *  Parses topics of the form: $share/<ShareName>/<TopicFilter>
 *
 *  Parameters:
 *      subtopic  - the subscription/topic to tokenise
 *      local_sub - output: the topic without $share/group/ prefix
 *      topics    - output: array of topic segments (NULL-terminated)
 *      count     - output: number of segments
 *      sharename - output: share group name (NULL if not shared)
 *
 *  Returns:
 *      0  - on success
 *      -1 - on error (invalid parameters or memory allocation failed)
 ***************************************************************************/
int sub__topic_tokenise_v2(
    const char *subtopic,
    char **local_sub,
    char ***topics,
    int *count,
    const char **sharename
);

#ifdef __cplusplus
}
#endif
