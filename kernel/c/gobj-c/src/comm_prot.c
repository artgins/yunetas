/***********************************************************************
 *          COMM_PROT.C
 *
 *          Communication protocols register
 *
 *          Copyright (c) 2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include "helpers.h"
#include "comm_prot.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Structures
 ***************************************************************/
typedef struct {
    DL_ITEM_FIELDS

    gclass_name_t gclass_name;
    const char *schema;
} comm_prot_t;

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE char __initialized__ = FALSE;
PRIVATE dl_list_t dl_comm_prot;

/***************************************************************************
 *  Register a gclass with a communication protocol
 ***************************************************************************/
PUBLIC int comm_prot_register(gclass_name_t gclass_name, const char *schema)
{
    if(!__initialized__) {
        __initialized__ = TRUE;
        dl_init(&dl_comm_prot);
    }

    comm_prot_t *lh = GBMEM_MALLOC(sizeof(comm_prot_t));
    if(!lh) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "No memory to comm_prot_t",
            NULL
        );
        return -1;
    }
    lh->schema = GBMEM_STRDUP(schema);
    lh->gclass_name = gclass_name;

    /*----------------*
     *  Add to list
     *----------------*/
    return dl_add(&dl_comm_prot, lh);
}

/***************************************************************************
 *  Get the gclass name implementing the schema
 ***************************************************************************/
PUBLIC gclass_name_t comm_prot_get_gclass(const char *schema)
{
    comm_prot_t *lh = dl_first(&dl_comm_prot);
    while(lh) {
        comm_prot_t *next = dl_next(lh);
        if(strcmp(lh->schema, schema)==0) {
            return lh->gclass_name;
        }
        /*
         *  Next
         */
        lh = next;
    }

    gobj_log_error(0, LOG_OPT_TRACE_STACK,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL_ERROR,
        "msg",          "%s", "gclass for schema not found",
        "schema",       "%s", schema,
        NULL
    );
    return NULL;
}

/***************************************************************************
 *  Free comm_prot register
 ***************************************************************************/
PUBLIC void comm_prot_free(void)
{
    comm_prot_t *lh;
    while((lh=dl_first(&dl_comm_prot))) {
        dl_delete(&dl_comm_prot, lh, 0);
        GBMEM_FREE(lh->schema)
        GBMEM_FREE(lh)
    }

    __initialized__ = FALSE;
}
