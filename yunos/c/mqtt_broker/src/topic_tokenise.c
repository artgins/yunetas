/****************************************************************************
 *          topic_tokenise.c
 *
 *          Rewrite of Mosquitto's sub__topic_tokenise() using Yunetas
 *          split* functions from helpers.h
 *
 *          This version uses Yunetas split3() and split_free3() functions
 *          for cleaner, more maintainable code.
 *
 *          NOTE: We use split3() instead of split2() because:
 *          - split2() removes empty tokens:  "a//b" -> ["a", "b"]
 *          - split3() preserves empty tokens: "a//b" -> ["a", "", "b"]
 *
 *          For MQTT topics, empty segments ARE significant:
 *          - "/a/b/"  must produce ["", "a", "b", ""]
 *          - "a//b"   must produce ["a", "", "b"]
 *
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <gobj.h>
#include "topic_tokenise.h"

/***************************************************************************
 *  sub__topic_tokenise
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
 *          topics[0] = "" (empty string before first /)
 *          topics[1] = "a"
 *          topics[2] = "deep"
 *          topics[3] = "topic"
 *          topics[4] = "hierarchy"
 *          topics[5] = "" (empty string after last /)
 *          topics[6] = NULL (terminator)
 *
 *  Note: Empty segments are stored as "" (empty strings), not NULL.
 *        NULL is used only as the array terminator.
 *        This allows sub__topic_tokens_free() to work without a count.
 *
 *  Parameters:
 *      subtopic - the subscription/topic to tokenise
 *      topics   - a pointer to store the array of strings (NULL-terminated)
 *      count    - an int pointer to store the number of items in the topics array
 *
 *  Returns:
 *      0  - on success
 *      -1 - on error (invalid parameters or memory allocation failed)
 ***************************************************************************/
int sub__topic_tokenise(const char *subtopic, char ***topics, int *count)
{
    /*
     *  Input validation
     */
    if(!subtopic || !topics || !count) {
        return -1;
    }

    /*
     *  Use Yunetas split3() to split the topic by '/' delimiter
     *
     *  split3() signature:
     *      const char **split3(const char *str, const char *delim, int *nelements)
     *
     *  - Returns a NULL-terminated array of strings
     *  - PRESERVES empty segments (critical for MQTT topics!)
     *  - nelements receives the count of elements
     *  - Memory must be freed with split_free3()
     *
     *  Why split3() and not split2():
     *  - split2() removes empty tokens:  "/a/b/" -> ["a", "b"]     (WRONG for MQTT)
     *  - split3() keeps empty tokens:    "/a/b/" -> ["", "a", "b", ""]  (CORRECT)
     */
    int nelements = 0;
    const char **split_result = split3(subtopic, "/", &nelements);

    if(!split_result) {
        return -1;
    }

    /*
     *  Allocate the output array with one extra slot for NULL terminator
     *
     *  Note: We allocate nelements + 1 to have a sentinel NULL at the end
     *  This allows sub__topic_tokens_free() to work without a count parameter
     */
    char **result = gbmem_calloc(nelements + 1, sizeof(char *));
    if(!result) {
        split_free3(split_result);
        return -1;
    }

    /*
     *  Copy each segment
     *
     *  Empty segments are stored as "" (empty strings), not NULL.
     *  This allows the array to be NULL-terminated.
     *
     *  Empty segment detection: check if topics[i][0] == '\0'
     */
    for(int i = 0; i < nelements; i++) {
        if(split_result[i]) {
            result[i] = gbmem_strdup(split_result[i]);
        } else {
            result[i] = gbmem_strdup("");
        }

        if(!result[i]) {
            /*
             *  Memory allocation failed - clean up and return error
             */
            for(int j = 0; j < i; j++) {
                gbmem_free(result[j]);
            }
            gbmem_free(result);
            split_free3(split_result);
            return -1;
        }
    }

    /*
     *  result[nelements] is already NULL from gbmem_calloc (sentinel)
     */

    /*
     *  Free the split result (we've copied what we need)
     */
    split_free3(split_result);

    /*
     *  Set output parameters
     */
    *topics = result;
    *count = nelements;

    return 0;
}

/***************************************************************************
 *  sub__topic_tokens_free
 *
 *  Free memory that was allocated in sub__topic_tokenise
 *
 *  The topics array is NULL-terminated. Empty segments are stored as ""
 *  (empty strings), not NULL, so we can safely iterate until NULL.
 *
 *  Parameters:
 *      topics - pointer to string array (NULL-terminated)
 *
 *  Returns:
 *      0  - on success
 *      -1 - on error (invalid parameters)
 ***************************************************************************/
int sub__topic_tokens_free(char ***topics)
{
    if(!topics || !(*topics)) {
        return -1;
    }

    char **t = *topics;

    /*
     *  Iterate until we find the NULL terminator
     *  All entries (including empty segments) are valid strings
     */
    for(int i = 0; t[i] != NULL; i++) {
        gbmem_free(t[i]);
    }

    gbmem_free(t);
    *topics = NULL;

    return 0;
}

/***************************************************************************
 *  sub__topic_tokenise_v2
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
)
{
    if(!subtopic || !topics || !count) {
        return -1;
    }

    /*
     *  Initialize outputs
     */
    if(local_sub) {
        *local_sub = NULL;
    }
    if(sharename) {
        *sharename = NULL;
    }
    *topics = NULL;
    *count = 0;

    /*
     *  Check for shared subscription prefix: $share/groupname/topic
     */
    const char *topic_start = subtopic;

    if(strncmp(subtopic, "$share/", 7) == 0) {
        /*
         *  This is a shared subscription
         *  Format: $share/<ShareName>/<TopicFilter>
         *
         *  Use split3() to get all segments (preserving empty ones)
         */
        int nelements = 0;
        const char **parts = split3(subtopic, "/", &nelements);

        if(!parts || nelements < 3) {
            if(parts) {
                split_free3(parts);
            }
            return -1;  // Invalid shared subscription format
        }

        /*
         *  parts[0] = "$share"
         *  parts[1] = ShareName
         *  parts[2...] = TopicFilter segments
         */

        if(sharename) {
            *sharename = gbmem_strdup(parts[1]);
            if(!(*sharename)) {
                split_free3(parts);
                return -1;
            }
        }

        /*
         *  Build local_sub: the topic without $share/groupname/
         *  Skip "$share" and ShareName, join the rest with /
         */
        if(local_sub) {
            size_t len = strlen(subtopic) - 7 - strlen(parts[1]) - 1;
            if(len > 0) {
                /*
                 *  Find the start of the actual topic filter
                 */
                const char *p = subtopic + 7;  // Skip "$share/"
                p = strchr(p, '/');            // Skip to after ShareName
                if(p) {
                    p++;  // Skip the '/'
                    *local_sub = gbmem_strdup(p);
                    if(!(*local_sub)) {
                        if(sharename && *sharename) {
                            gbmem_free((char *)*sharename);
                            *sharename = NULL;
                        }
                        split_free3(parts);
                        return -1;
                    }
                    topic_start = *local_sub;
                }
            }
        }

        split_free3(parts);

        /*
         *  Now tokenise the actual topic filter
         */
        if(topic_start) {
            return sub__topic_tokenise(topic_start, topics, count);
        }

        return 0;

    } else {
        /*
         *  Not a shared subscription - just tokenise normally
         */
        if(local_sub) {
            *local_sub = gbmem_strdup(subtopic);
            if(!(*local_sub)) {
                return -1;
            }
        }

        return sub__topic_tokenise(subtopic, topics, count);
    }
}

/***************************************************************************
 *  Example usage / test code
 ***************************************************************************/
#ifdef TEST_TOPIC_TOKENISE

#include <stdio.h>

static void print_tokens(const char *topic, char **tokens, int count)
{
    printf("Topic: \"%s\"\n", topic);
    printf("  Count: %d\n", count);
    for(int i = 0; i < count; i++) {
        if(tokens[i][0] == '\0') {
            printf("  [%d] = \"\" (empty segment)\n", i);
        } else {
            printf("  [%d] = \"%s\"\n", i, tokens[i]);
        }
    }
    printf("\n");
}

int main(void)
{
    char **topics;
    int count;
    int ret;

    /*
     *  Test cases
     */
    const char *test_topics[] = {
        "a/deep/topic/hierarchy",
        "/a/deep/topic/hierarchy/",
        "simple",
        "/",
        "//",
        "a//b",
        "$SYS/broker/uptime",
        "+/temperature/#",
        "$share/group1/sensor/+/temperature",
        NULL
    };

    printf("=== Testing sub__topic_tokenise with Yunetas split3 ===\n\n");

    for(int i = 0; test_topics[i] != NULL; i++) {
        ret = sub__topic_tokenise(test_topics[i], &topics, &count);
        if(ret == 0) {
            print_tokens(test_topics[i], topics, count);
            sub__topic_tokens_free(&topics);
        } else {
            printf("Topic: \"%s\" - ERROR: %d\n\n", test_topics[i], ret);
        }
    }

    /*
     *  Test the v2 version with shared subscriptions
     */
    printf("=== Testing sub__topic_tokenise_v2 (shared subscriptions) ===\n\n");

    const char *shared_topic = "$share/mygroup/sensor/+/temperature";
    char *local_sub = NULL;
    const char *sharename = NULL;

    ret = sub__topic_tokenise_v2(shared_topic, &local_sub, &topics, &count, &sharename);
    if(ret == 0) {
        printf("Shared Topic: \"%s\"\n", shared_topic);
        printf("  ShareName: \"%s\"\n", sharename ? sharename : "(null)");
        printf("  LocalSub: \"%s\"\n", local_sub ? local_sub : "(null)");
        print_tokens(local_sub ? local_sub : shared_topic, topics, count);

        sub__topic_tokens_free(&topics);
        if(local_sub) {
            gbmem_free(local_sub);
        }
        if(sharename) {
            gbmem_free((char *)sharename);
        }
    }

    return 0;
}

#endif /* TEST_TOPIC_TOKENISE */
