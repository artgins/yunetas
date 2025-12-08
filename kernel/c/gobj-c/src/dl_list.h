/****************************************************************************
 *              dl_list.h
 *
 *              Double link list functions.
 *              Independent functions of any C library.
 *
 *              Copyright (c) 1994-2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gtypes.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Structures
 ***************************************************************/
typedef void (*fnfree)(void *);

#define DL_ITEM_FIELDS              \
    struct dl_list_s *__dl__;       \
    struct dl_item_s  *__next__;    \
    struct dl_item_s  *__prev__;    \
    size_t __id__;

typedef struct dl_item_s {
    DL_ITEM_FIELDS
} dl_item_t;

typedef struct dl_list_s {
    struct dl_item_s *head;
    struct dl_item_s *tail;
    size_t __itemsInContainer__;
    size_t __last_id__; // auto-incremental, always.
    void *gobj;
} dl_list_t;

/**************************************************************
 *       Prototypes
 **************************************************************/
PUBLIC int dl_init(dl_list_t *dl, hgobj gobj);

static inline void *dl_first(dl_list_t *dl) /* Seek first item */
{
    return dl->head;
}

static inline void *dl_last(dl_list_t *dl) /* Seek last item */
{
    return dl->tail;
}

static inline void *dl_next(void *curr) /* next Item */
{
    if(curr) {
        return ((dl_item_t *) curr)->__next__;
    }
    return (void *)0;
}

static inline void *dl_prev(void *curr) /* previous Item */
{
    if(curr) {
        return ((dl_item_t *) curr)->__prev__;
    }
    return (void *)0;
}

PUBLIC int dl_insert(dl_list_t *dl, void *item);
PUBLIC int dl_add(dl_list_t *dl, void *item);
PUBLIC void * dl_find(dl_list_t *dl, void *item);
PUBLIC void * dl_nfind(dl_list_t *dl, size_t nitem); /* Find from head by number, relative to 1 */
PUBLIC int dl_delete(dl_list_t *dl, void * curr_, void (*fnfree)(void *));
PUBLIC int dl_delete_item(void *curr, void (*fnfree)(void *));
PUBLIC void dl_flush(dl_list_t *dl, void (*fnfree)(void *));

static inline size_t dl_size(dl_list_t *dl) /* Return number of items in list */
{
    if(!dl) {
        return 0;
    }
    return dl->__itemsInContainer__;
}

/****************************************************************************
 *              FOREACH Macros
 ****************************************************************************/

/**
 *  FOREACH - Iterate over all items in a double linked list
 *
 *  @param dl       Pointer to dl_list_t
 *  @param item     Iterator variable (must be a pointer type matching your items)
 *
 *  Example:
 *      dl_list_t *list;
 *      my_item_t *item;
 *      FOREACH(list, item) {
 *          // use item
 *      }
 */
#define DL_FOREACH(dl, item) \
    for((item) = dl_first(dl); \
        (item) != NULL; \
        (item) = dl_next(item))

/**
 *  FOREACH_SAFE - Safely iterate over all items in a double linked list
 *  Allows deletion of current item during iteration
 *
 *  @param dl       Pointer to dl_list_t
 *  @param item     Iterator variable (must be a pointer type matching your items)
 *  @param next     Temporary variable to store next item (same type as item)
 *
 *  Example:
 *      dl_list_t *list;
 *      my_item_t *item;
 *      my_item_t *next;
 *      FOREACH_SAFE(list, item, next) {
 *          if(condition) {
 *              dl_delete_item(item, free);  // Safe to delete current item
 *          }
 *      }
 */
#define DL_FOREACH_SAFE(dl, item, next) \
    for((item) = dl_first(dl), (next) = (item) ? dl_next(item) : NULL; \
        (item) != NULL; \
        (item) = (next), (next) = (item) ? dl_next(item) : NULL)


#ifdef __cplusplus
}
#endif
