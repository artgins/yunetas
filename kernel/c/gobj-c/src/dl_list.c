/****************************************************************************
 *              dl_list.c
 *
 *              Double link list functions.
 *              Independent functions of any C library.
 *
 *              Copyright (c) 1994-2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include "glogger.h"
#include "dl_list.h"

/***************************************************************
 *      Initialize double list
 ***************************************************************/
PUBLIC int dl_init(dl_list_t *dl, hgobj gobj)
{
    if(dl->head || dl->tail || dl->__itemsInContainer__) {
        gobj_trace_msg(0, "dl_init(): Wrong dl_list_t, MUST be empty");
        abort();
    }
    dl->head = 0;
    dl->tail = 0;
    dl->__itemsInContainer__ = 0;
    dl->gobj = gobj;
    return 0;
}

/***************************************************************
 *  Check if a new item has links: MUST have NO links
 *  Return TRUE if it has links
 ***************************************************************/
PRIVATE BOOL check_links(register dl_item_t *item)
{
    if(item->__prev__ || item->__next__ || item->__dl__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_item_t, WITH links",
            NULL
        );
        return TRUE;
    }
    return FALSE;
}

/***************************************************************
 *  Check if a item has no links: MUST have links
 *  Return TRUE if it has not links
 ***************************************************************/
PRIVATE BOOL check_no_links(register dl_item_t *item)
{
    if(!item->__dl__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_item_t, WITHOUT links",
            NULL
        );
        return TRUE;
    }
    return FALSE;
}

/***************************************************************
 *   Insert at the head
 ***************************************************************/
PUBLIC int dl_insert(dl_list_t *dl, void *item)
{
    if(check_links(item)) return -1;

    if(dl->head==0) { /*---- Empty List -----*/
        ((dl_item_t *)item)->__prev__=0;
        ((dl_item_t *)item)->__next__=0;
        dl->head = item;
        dl->tail = item;
    } else  { /* FIRST ITEM */
        ((dl_item_t *)item)->__prev__ = 0;
        ((dl_item_t *)item)->__next__ = dl->head;
        dl->head->__prev__ = item;
        dl->head = item;
    }

    dl->__itemsInContainer__++;
    dl->__last_id__++;
    ((dl_item_t *)item)->__id__ = dl->__last_id__;
    ((dl_item_t *)item)->__dl__ = dl;

    return 0;
}

/***************************************************************
 *      Add at end of the list
 ***************************************************************/
PUBLIC int dl_add(dl_list_t *dl, void *item)
{
    if(check_links(item)) return -1;

    if(dl->tail==0) { /*---- Empty List -----*/
        ((dl_item_t *)item)->__prev__=0;
        ((dl_item_t *)item)->__next__=0;
        dl->head = item;
        dl->tail = item;
    } else { /* LAST ITEM */
        ((dl_item_t *)item)->__prev__ = dl->tail;
        ((dl_item_t *)item)->__next__ = 0;
        dl->tail->__next__ = item;
        dl->tail = item;
    }
    dl->__itemsInContainer__++;
    dl->__last_id__++;
    ((dl_item_t *)item)->__id__ = dl->__last_id__;
    ((dl_item_t *)item)->__dl__ = dl;

    return 0;
}

/***************************************************************
 *      dl_find - find forward
 ***************************************************************/
PUBLIC void * dl_find(dl_list_t *dl, void *item)
{
    register dl_item_t * curr;

    curr = dl->head;

    while(curr != 0)  {
        if(curr==item)
            return item;
        curr = curr->__next__;
    }
    return (void *)0; /* not found */
}

/***************************************************************
 *    Delete current item
 ***************************************************************/
PUBLIC int dl_delete(dl_list_t *dl, void * curr_, void (*fnfree)(void *))
{
    register dl_item_t * curr = curr_;
    /*-------------------------*
     *     Check nulls
     *-------------------------*/
    if(curr==0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item NULL",
            NULL
        );
        return -1;
    }
    if(check_no_links(curr))
        return -1;

    if(curr->__dl__ != dl) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item with DIFFERENT dl_list_t",
            NULL
        );
        return -1;
    }

    /*-------------------------*
     *     Check list
     *-------------------------*/
    if(dl->head==0 || dl->tail==0 || dl->__itemsInContainer__==0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Deleting item in EMPTY list",
            NULL
        );
        return -1;
    }

    /*------------------------------------------------*
     *                 Delete
     *------------------------------------------------*/
    if(curr->__prev__==0) {
        /*------------------------------------*
         *  FIRST ITEM. (curr==dl->head)
         *------------------------------------*/
        dl->head = dl->head->__next__;
        if(dl->head) /* is last item? */
            dl->head->__prev__=0; /* no */
        else
            dl->tail=0; /* yes */

    } else {
        /*------------------------------------*
         *    MIDDLE or LAST ITEM
         *------------------------------------*/
        curr->__prev__->__next__ = curr->__next__;
        if(curr->__next__) /* last? */
            curr->__next__->__prev__ = curr->__prev__; /* no */
        else
            dl->tail= curr->__prev__; /* yes */
    }

    /*-----------------------------*
     *  Decrement items counter
     *-----------------------------*/
    dl->__itemsInContainer__--;

    /*-----------------------------*
     *  Reset pointers
     *-----------------------------*/
    curr->__prev__ = 0;
    curr->__next__ = 0;
    curr->__dl__ = 0;

    /*-----------------------------*
     *  Free item
     *-----------------------------*/
    if(fnfree) {
        (*fnfree)(curr);
    }

    return 0;
}

/***************************************************************
 *      DL_LIST: Flush double list. (Remove all items).
 ***************************************************************/
PUBLIC void dl_flush(dl_list_t *dl, void (*fnfree)(void *))
{
    register dl_item_t *first;

    while((first = dl_first(dl))) {
        dl_delete(dl, first, fnfree);
    }
    if(dl->head || dl->tail || dl->__itemsInContainer__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "Wrong dl_list_t, MUST be empty",
            NULL
        );
    }
}
