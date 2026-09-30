/****************************************************************************
 *          poison_alloc.c
 *
 *          Freed memory that stays freed, for the tests of emailsender.
 *
 *          A pointer read after its owner was freed usually reads what it
 *          read before: the block is handed out again at once, and often to
 *          a copy of the very same thing -- a queue closed and opened again
 *          loads the same message into the same block, and a dangling
 *          q_msg_t looks right. So every block freed is filled with
 *          POISON_BYTE and held in a quarantine of QUARANTINE_SIZE blocks
 *          before it is given back, and a read after the free reads the
 *          poison. Built over the allocators gbmem had, installed before
 *          anything is allocated (json included: the entry point hands
 *          jansson the allocators it finds). The same technique as
 *          tests/c/c_controlcenter_scenarios.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include "poison_alloc.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define POISON_BYTE     0x5A
#define QUARANTINE_SIZE 4096

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    size_t size;
    size_t pad;     // keeps the block 16-aligned
} poison_hdr_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE sys_malloc_fn_t base_malloc;
PRIVATE sys_realloc_fn_t base_realloc;
PRIVATE sys_calloc_fn_t base_calloc;
PRIVATE sys_free_fn_t base_free;
PRIVATE poison_hdr_t *quarantine[QUARANTINE_SIZE];
PRIVATE size_t quarantine_idx = 0;
PRIVATE BOOL quarantine_closed = FALSE;   // from the end of the run: freed at once

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void *poison_malloc(size_t size)
{
    poison_hdr_t *hdr = base_malloc(sizeof(poison_hdr_t) + size);
    if(!hdr) {
        return NULL;    // Error already logged
    }
    memset(hdr + 1, 0, size);
    hdr->size = size;
    return hdr + 1;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void poison_free(void *p)
{
    if(!p) {
        return;
    }
    poison_hdr_t *hdr = ((poison_hdr_t *)p) - 1;
    memset(p, POISON_BYTE, hdr->size);
    if(quarantine_closed) {
        base_free(hdr);
        return;
    }
    poison_hdr_t *old = quarantine[quarantine_idx];
    quarantine[quarantine_idx] = hdr;
    quarantine_idx = (quarantine_idx + 1) % QUARANTINE_SIZE;
    if(old) {
        base_free(old);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void *poison_realloc(void *p, size_t size)
{
    if(!p) {
        return poison_malloc(size);
    }
    poison_hdr_t *hdr = ((poison_hdr_t *)p) - 1;
    void *np = poison_malloc(size);
    if(!np) {
        return NULL;    // Error already logged
    }
    memcpy(np, p, hdr->size < size? hdr->size : size);
    poison_free(p);
    return np;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void *poison_calloc(size_t n, size_t size)
{
    return poison_malloc(n * size);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void install_poison_allocators(void)
{
    gbmem_get_allocators(&base_malloc, &base_realloc, &base_calloc, &base_free);
    gbmem_set_allocators(poison_malloc, poison_realloc, poison_calloc, poison_free);
}

/***************************************************************************
 *  The blocks held are freed memory: counted as not free, they printed a
 *  false leak report on every run, which would hide a real one.
 ***************************************************************************/
PUBLIC void release_quarantine(void)
{
    quarantine_closed = TRUE;
    for(size_t i=0; i<QUARANTINE_SIZE; i++) {
        if(quarantine[i]) {
            base_free(quarantine[i]);
            quarantine[i] = NULL;
        }
    }
}
