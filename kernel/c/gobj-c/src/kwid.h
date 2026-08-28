/****************************************************************************
 *              kwid.h
 *
 *              kw helpers
 *
 *              Copyright (c) 2014,2023 Niyamaka.
 *              Copyright (c) 2024,2026 ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

/*
 *  Dependencies
 */
#include "helpers.h"

#ifdef __cplusplus
extern "C"{
#endif

/*****************************************************************
 *     Constants
 *****************************************************************/

/*****************************************************************
 *     Prototypes
 *****************************************************************/
/*---------------------------------*
 *          KW0
 *---------------------------------*/
/*---------------------------------*
 *      Json functions
 *---------------------------------*/
#define JSON_DECREF(json)                                           \
{                                                                   \
    if(json) {                                                      \
        if((((int)((json)->refcount)) <= 0 && (json)->type < JSON_TRUE) || \
                (json)->type > 7 || (json)->type < 0) { \
            gobj_log_error(0, LOG_OPT_TRACE_STACK,                  \
                "gobj",         "%s", __FILE__,                     \
                "function",     "%s", __FUNCTION__,                 \
                "msgset",       "%s", MSGSET_INTERNAL,        \
                "msg",          "%s", "BAD json_decref()",          \
                "refcount",     "%d", (int)((json)->refcount),      \
                "type",         "%d", (int)((json)->type),          \
                NULL                                                \
            );                                                      \
        } else {                                                    \
            json_decref(json);                                      \
        }                                                           \
        (json) = 0;                                                 \
    }                                                               \
}
#define JSON_INCREF(json)                                           \
{                                                                   \
    if(json) {                                                      \
        if((((int)((json)->refcount)) <= 0 && (json)->type < JSON_TRUE) || \
                (json)->type > 7 || (json)->type < 0) { \
            gobj_log_error(0, LOG_OPT_TRACE_STACK,                  \
                "gobj",         "%s", __FILE__,                     \
                "function",     "%s", __FUNCTION__,                 \
                "msgset",       "%s", MSGSET_INTERNAL,        \
                "msg",          "%s", "BAD json_incref()",          \
                "refcount",     "%d", (int)((json)->refcount),      \
                "type",         "%d", (int)((json)->type),          \
                NULL                                                \
            );                                                      \
        } else {                                                    \
            json_incref(json);                                      \
        }                                                           \
    }                                                               \
}

#define json_array_backward(array, index, value) \
    for(index = json_array_size(array) - 1; \
        index >= 0 && (value = json_array_get(array, index)); \
        index--)

#define json_array_foreach_continue(array, index, value)                                \
    for (;                                                                              \
         index < json_array_size(array) && (value = json_array_get(array, index));      \
         index++)

#define JSON_TYPEOF(json, tipo) (json && json_typeof(json)==tipo)

/*---------------------------------*
 *          KW
 *---------------------------------*/
typedef void *(*incref_fn_t)(void *);
typedef void (*decref_fn_t)(void *);
typedef json_t * (*serialize_fn_t)(hgobj gobj, void *ptr);
typedef void * (*deserialize_fn_t)(hgobj gobj, json_t *jn);

#define KW_DECREF(ptr)      \
    if(ptr) {               \
        kw_decref(ptr);     \
        (ptr) = 0;          \
    }

#define KW_INCREF(ptr)      \
    if(ptr) {               \
        kw_incref(ptr);     \
    }

typedef enum {
    KW_REQUIRED         = 0x0001,   // Log error message if not exist.
    KW_CREATE           = 0x0002,   // Create if not exist
    KW_WILD_NUMBER      = 0x0004,   // For numbers work with real/int/bool/string without error logging
    KW_EXTRACT          = 0x0008,   // Extract (delete) the key on read from dictionary.
    KW_BACKWARD         = 0x0010,   // Search backward in lists or arrays
    KW_VERBOSE          = 0x0020,   //
    KW_LOWER            = 0x0040,   //
    KW_RECURSIVE        = 0x0080,   //
} kw_flag_t;

PUBLIC int kw_add_binary_type(
    const char *binary_field_name,
    const char *serialized_field_name,
    serialize_fn_t serialize_fn,
    deserialize_fn_t deserialize_fn,
    incref_fn_t incref_fn,
    decref_fn_t decref_fn
);
PUBLIC json_t *kw_serialize(
    hgobj gobj,
    json_t *kw // like owned, return same kw
);
PUBLIC char *kw_serialize_to_string( // return must be free with jsonp_free()
    hgobj gobj,
    json_t *kw
);
PUBLIC json_t *kw_deserialize(
    hgobj gobj,
    json_t *kw // like owned, return same kw
);
PUBLIC json_t *kw_incref(json_t *kw);
PUBLIC json_t *kw_decref(json_t* kw);

PUBLIC BOOL kw_has_key(json_t *kw, const char *key);

/**rst**
   Set delimiter. Default is '`'.
   Return previous delimiter
**rst**/
PUBLIC char kw_set_path_delimiter(char delimiter);

/**rst**
    Return the json value find by path
    Walk over dicts and lists
**rst**/
PUBLIC json_t *kw_find_path(hgobj gobj, json_t *kw, const char *path, BOOL verbose);

/**rst**
    Like json_object_set but with a path (doesn't create arrays, only objects)
**rst**/
PUBLIC int kw_set_dict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,   // The last word after delimiter (.) is the key
    json_t *value // owned
);

PUBLIC int kw_set_subdict_value(
    hgobj gobj,
    json_t* kw,
    const char *path,
    const char *key,
    json_t *value // owned
);

/**rst**
   Delete value searched by path
**rst**/
PUBLIC int kw_delete(
    hgobj gobj,
    json_t *kw,
    const char *path
);

/**rst**
   Delete sub-key
**rst**/
PUBLIC int kw_delete_subkey(hgobj gobj, json_t *kw, const char *path, const char *key);

/**rst**
    Get a the idx of string in a json list.
    Return -1 if not found
**rst**/
PUBLIC int kw_find_str_in_list(
    hgobj gobj,
    json_t *kw_list,
    const char *str
);

/**rst**
    Utility for databases.
    Being field `kw` a list of id record [{id...},...] return the record idx with `id`
    Return -1 if not found
**rst**/
int kwid_find_record_in_list(
    hgobj gobj,
    json_t *kw_list,
    const char *id,
    kw_flag_t flag
);

/**rst**
    Get a the idx of simple json item in a json list.
    Return -1 if not found
**rst**/
PUBLIC int kw_find_json_in_list(
    hgobj gobj,
    json_t *kw_list,  // not owned
    json_t *item,  // not owned
    kw_flag_t flag
);

/**rst**
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of **duplicated** kw filtering the rows by `jn_filter` (where),
    If match_fn is 0 then kw_match_simple is used.
**rst**/
PUBLIC json_t *kw_select( // WARNING return **duplicated** objects
    hgobj gobj,
    json_t *kw,         // NOT owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // NOT owned
        json_t *jn_filter   // owned
    )
);

/**rst**
    Being `kw` a row's list or list of dicts [{},...],
    return a new list of incref (clone) kw filtering the rows by `jn_filter` (where),
    If match_fn is 0 then kw_match_simple is used.
**rst**/
PUBLIC json_t *kw_collect(
    hgobj gobj,
    json_t *kw,         // NOT owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // NOT owned
        json_t *jn_filter   // owned
    )
);


/**rst**
    Compare deeply two json values. Can be dicts, lists, or simple values.
    Dicts and lists can be disordered.
**rst**/
PUBLIC BOOL kwid_compare_records(
    hgobj gobj,
    json_t *record, // NOT owned
    json_t *expected, // NOT owned
    const char **ignore_keys,
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
);

/**rst**
    Compare deeply two json lists. Items can be disordered.
    Handles arrays of objects (with or without "id" field),
    arrays of arrays, and arrays of simple values.
**rst**/
PUBLIC BOOL kwid_compare_lists(
    hgobj gobj,
    json_t *list, // NOT owned
    json_t *expected, // NOT owned
    const char **ignore_keys,
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
);

/**rst**
   Return the ``path` dict value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_dict(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` list value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_list(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,
    kw_flag_t flag
);

/**rst**
   Get the value in idx position from an json array .
   Return the ``idx` json value from the ``kw`` list.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
**rst**/
PUBLIC json_t *kw_get_list_value(
    hgobj gobj,
    json_t* kw,
    int idx,
    kw_flag_t flag
);

/**rst**
   Return the ``path` int value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_int_t kw_get_int(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_int_t default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` real value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC double kw_get_real(
    hgobj gobj,
    json_t *kw,
    const char *path,
    double default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` boolean value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC BOOL kw_get_bool(
    hgobj gobj,
    json_t *kw,
    const char *path,
    BOOL default_value,
    kw_flag_t flag
);

/**rst**
   Return the ``path` string value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC const char *kw_get_str(
    hgobj gobj,
    json_t *kw,
    const char *path,
    const char *default_value,
    kw_flag_t flag
);

/**rst**
   Get any value from an json object searched by path.
   Return the ``path` json value from the ``kw`` dict.
   If it's ``KW_REQUIRED`` and ``path`` not exist then log an error.
   If ``path`` doesn't exist then return the ``default_value``.
**rst**/
PUBLIC json_t *kw_get_dict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,
    json_t *default_value,  // owned
    kw_flag_t flag
);

PUBLIC json_t *kw_get_subdict_value(
    hgobj gobj,
    json_t *kw,
    const char *path,
    const char *key,
    json_t *jn_default_value,  // owned
    kw_flag_t flag
);

PUBLIC void kw_update_except(
    hgobj gobj,
    json_t *kw,  // not owned
    json_t *other,  // owned
    const char **except_keys
);

/**rst**
    Compare two json and return TRUE if they are identical.
**rst**/
#define kw_is_identical json_is_identical

/************************************************************************
    WARNING

    **duplicated** is a copy with new references
            to duplicate you must use json_deep_copy()

    **clone** is a copy with incref references

 ************************************************************************/

/**rst**
   Make a duplicate of kw
   WARNING near as json_deep_copy(), but processing serialized fields and with new references
**rst**/
PUBLIC json_t *kw_duplicate(
    hgobj gobj,
    json_t *kw  // NOT owned
);


/**rst**
    Return a new kw only with the keys got by path.
    It's not a deep copy, new keys are the paths.
    Not valid with lists.
    If paths are empty return kw
**rst**/
PUBLIC json_t *kw_clone_by_path(
    hgobj gobj,
    json_t *kw,     // owned
    const char **paths
);

/**rst**
    Return a new kw only with the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return kw
**rst**/
PUBLIC json_t *kw_clone_by_keys(
    hgobj gobj,
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
);

/**rst**
    Return a new kw except the keys got by dict's keys or list's keys (strings).
    Keys:
        "$key"
        ["$key1", "$key2", ...]
        {"$key1":*, "$key2":*, ...}

    It's not a deep copy, new keys are the paths.
    If paths are empty return kw
**rst**/
PUBLIC json_t *kw_clone_by_not_keys(
    hgobj gobj,
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL verbose
);

/**rst**
    Remove from kw1 all keys in kw2
    kw2 can be a string, dict, or list.
**rst**/
PUBLIC int kw_pop(
    json_t *kw1, // NOT owned
    json_t *kw2  // NOT owned
);

/**rst**
    Match a json dict with a json filter
    If jn_filter is null or empty the match is true
**rst**/
PUBLIC BOOL kw_match_simple(
    json_t *kw,         // NOT owned
    json_t *jn_filter   // owned
);
typedef BOOL (*kw_match_fn)(
    json_t *kw,         // NOT owned
    json_t *jn_filter   // owned
);

/**rst**
    HACK Convention: private data begins with "_".
    Delete private keys (only first level)
**rst**/
PUBLIC int kw_delete_private_keys(
    json_t *kw  // NOT owned
);

/**rst**
    HACK Convention: metadata begins with "__".
    Delete metadata keys (only first level)
**rst**/
PUBLIC int kw_delete_metadata_keys(
    json_t *kw  // NOT owned
);

PUBLIC int kw_walk(
    hgobj gobj,
    json_t *kw, // not owned
    int (*callback)(hgobj gobj, json_t *kw, const char *key, json_t *value)
);


/*---------------------------------*
 *          KWID
 *---------------------------------*/




/***************************************************************************
    Utility for databases.
    Get a json list or dict, get the **first** record that match `id`
    Convention:
        - If it's a list of dict: the records have "id" field as primary key
        - If it's a dict, the key is the `id`
 ***************************************************************************/
/**rst**
    Return a new json with all arrays or dicts greater than `limit` collapsed with
        __collapsed__: {
            path:
            size:
        }
    `kw` can be a dict or a top-level array; anything else is refused with NULL.
    A top-level array collapses its object elements one level, and their paths
    are the numeric index, which kw_find_path() resolves back.
    WARNING _limit must be > 0 to collapse
**rst**/
PUBLIC json_t *kw_collapse(
    hgobj gobj,
    json_t *kw,         // not owned
    int collapse_lists_limit,
    int collapse_dicts_limit
);

PUBLIC json_t *kwid_get(
    hgobj gobj,
    json_t *kw,  // NOT owned
    kw_flag_t flag,
    const char *path,
    ...
) JANSSON_ATTRS((format(printf, 4, 5)));

/**rst**
    Utility for databases.
    Return a new list from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to their key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'
    If path is empty then use kw
**rst**/
PUBLIC json_t *kwid_new_list(
    hgobj gobj,
    json_t *kw,  // NOT owned
    kw_flag_t flag,
    const char *path,
    ...
) JANSSON_ATTRS((format(printf, 4, 5)));

/**rst**
    Utility for databases.
    Return a new dict from a "dict of records" or "list of records"
    WARNING the "id" of a dict's record is hardcorded to their key.
    Convention:
        - all arrays are list of records (dicts) with "id" field as primary key
        - delimiter is '`' and '.'
    If path is empty then use kw
**rst**/
PUBLIC json_t *kwid_new_dict(
    hgobj gobj,
    json_t *kw,  // NOT owned
    kw_flag_t flag,
    const char *path,
    ...
) JANSSON_ATTRS((format(printf, 4, 5)));

/**rst**
    HACK Convention: private data begins with "_".
    This function return a duplicate of kw removing all private data
**rst**/
PUBLIC json_t *kw_filter_private(
    hgobj gobj,
    json_t *kw  // owned
);

/**rst**
    HACK Convention: metadata begins with "__".
    This function return a duplicate of kw removing all metadata
**rst**/
PUBLIC json_t *kw_filter_metadata(
    hgobj gobj,
    json_t *kw  // owned
);

PUBLIC size_t kw_size(json_t *kw); // size of dict or size of list, remains return 1

/***************************************************************************
    Utility for databases of json records.
    Get a json list or dict, get the **first** record that match `id`
    WARNING `id` is the first key of json_desc
    Convention:
        - If it's a list of dict: the records have "id" field as primary key
        - If it's a dict, the key is the `id`
 ***************************************************************************/
PUBLIC json_t *kwjr_get( // Return is NOT yours, unless use of KW_EXTRACT
    hgobj gobj,
    json_t *kw,  // NOT owned
    const char *id,
    json_t *new_record,
    const json_desc_t *json_desc,
    size_t *idx_,      // If not null set the idx in case of array
    kw_flag_t flag
);

/**rst**
    Utility for databases.
    Being `ids` a:
        "$id"
        ["$id", ...]
        [{"id":$id, ...}, ...]
        {
            "$id": {}.
            ...
        }
    return a new list of all ids (all duplicated items)
**rst**/
PUBLIC json_t *kwid_get_ids(
    json_t *ids // not owned
);

/***************************************************************************
    Has word? Can be in string, list or dict.
    options: "recursive", "verbose"

    Use to configurate:

        "opt2"              No has word "opt1"
        "opt1|opt2"         Yes, has word "opt1"
        ["opt1", "opt2"]    Yes, has word "opt1"
        {
            "opt1": true,   Yes, has word "opt1"
            "opt2": false   No has word "opt1"
        }

 ***************************************************************************/
// TODO review if these functions are used only in tranger
PUBLIC BOOL kw_has_word(
    hgobj gobj,
    json_t *kw,  // NOT owned
    const char *word,
    kw_flag_t kw_flag
);

/**rst**
    Utility for databases.
    Return TRUE if `id` is in the list/dict/str `ids`
**rst**/
PUBLIC BOOL kwid_match_id(
    hgobj gobj,
    json_t *ids,
    const char *id
);

/**rst**
    Utility for databases.
    Return TRUE if `id` WITH LIMITED SIZE is in the list/dict/str `ids`
**rst**/
PUBLIC BOOL kwid_match_nid(
    hgobj gobj,
    json_t *ids,
    const char *id,
    int max_id_size
);

/***************************************************************************
 *              FLAT JSON: a json seen as a table
 *
 *  One row per LEAF: the id is the PATH of the item, the value is its
 *  value. Easier to store, to compare and to diff than the nested form,
 *  and the only one a person can read when two configurations disagree.
 *
 *      {"a": {"b": 1}, "c": [10, 20]}
 *          ->  {"a`b": 1, "c`[0]": 10, "c`[1]": 20}
 *
 *  THE GRAMMAR, and the reason for each rule:
 *
 *      - segments are joined by '`', which is already the path delimiter
 *        of kw_get_dict() and friends.
 *      - a literal '`' inside a key is DOUBLED. With that, every key is
 *        representable and the form forbids nothing -- the previous
 *        implementation had to reserve all-digit keys for array indices,
 *        and this system's own yuno ids ("1630") broke that rule: a dict
 *        keyed by one came back as an ARRAY OF 1631 ELEMENTS.
 *      - an array index is '[N]', canonical (no leading zeros). One byte
 *        more than a bare number, and it buys the forbidden-key rule back.
 *      - a dict key that begins with '[' doubles it ('[[0]'), so it can
 *        never be read as an index.
 *      - AN EMPTY CONTAINER IS A LEAF: {} and [] hold no leaves, so a
 *        strict leaves-only form loses them, and 'properties': {} is
 *        everywhere in this system's configs and schemas.
 *
 *  flat2json() REFUSES instead of guessing: an id used as leaf and as
 *  container (the result would depend on the order the ids are read in),
 *  an index over the limit (one id would materialise a million nulls), a
 *  path deeper than the limit (the old code truncated in silence).
 ***************************************************************************/

/*
 *  Nested -> flat. Return is yours; NULL if the json is deeper than the
 *  limit.
 */
PUBLIC json_t *json2flat(json_t *jn_nested);

/*
 *  Flat -> nested. Return is yours; NULL on the FIRST id that does not
 *  fit, with 'error' saying which and why -- rebuilding "most of it" is
 *  how a configuration comes back subtly different from the one saved.
 */
PUBLIC json_t *flat2json(json_t *jn_flat, char *error, int error_size);

/*
 *  Compose / take apart a flat id. A segment is either a STRING (a dict
 *  key, escaped as needed) or an INTEGER (an array index, written '[N]').
 *
 *  THEY ARE TWO TYPES AND NOT TWO SPELLINGS: as strings, the key "[0]"
 *  and the index 0 would both be "[0]", which is the very ambiguity the
 *  '[N]' form removes.
 *
 *  join(): return is yours (gbmem_free). split(): return is yours (json
 *  array), NULL if the id is malformed.
 */
PUBLIC char *flat_key_join(json_t *jn_segments);
PUBLIC json_t *flat_key_split(const char *key);

/*
 *  What changed between two flat dicts:
 *
 *      {"added": {id: value}, "removed": {id: value},
 *       "changed": {id: {"from": value, "to": value}}}
 *
 *  Return is yours.
 */
PUBLIC json_t *flat_diff(json_t *jn_flat1, json_t *jn_flat2);

/*
 *  Apply that diff to a flat dict, which is MUTATED and not owned. It
 *  works on the flat form on purpose: there an id addresses one value, so
 *  applying is setting and deleting, with nothing to walk and nothing to
 *  guess. Nested is json2flat -> flat_apply -> flat2json.
 */
PUBLIC int flat_apply(json_t *jn_flat, json_t *jn_diff, char *error, int error_size);

/*
 *  DEPRECATED: json2flat() / flat2json() say it better and flat2json()
 *  can explain a failure. These two answer the CURRENT form, not the one
 *  they used to.
 */
PUBLIC json_t *json_flatten_dict(json_t *jn_nested);
PUBLIC json_t *json_unflatten_dict(json_t *jn_flat);

#ifdef __cplusplus
}
#endif
