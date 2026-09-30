/****************************************************************************
 *          poison_alloc.h
 *
 *          Freed memory that stays freed, for the tests of emailsender: a
 *          read after a free reads poison, every time.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  Install before anything is allocated, first thing in main().
 */
PUBLIC void install_poison_allocators(void);

/*
 *  Free the blocks held, and from now on free every block at once. Call it
 *  in cleaning(), before the entry point's memory check, and again after
 *  yuneta_entry_point() (an entry point that ended before cleaning()).
 */
PUBLIC void release_quarantine(void);

#ifdef __cplusplus
}
#endif
