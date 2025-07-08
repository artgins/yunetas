/****************************************************************************
 *              gbmem.h
 *
 *              Block Memory Core
 *
 *              Copyright (c) 1996-2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gtypes.h"

#ifdef __cplusplus
extern "C"{
#endif

/**************************************************************
 *       Constants
 **************************************************************/
typedef void * (*sys_malloc_fn_t)(size_t sz);
typedef void * (*sys_realloc_fn_t)(void * ptr, size_t sz);
typedef void * (*sys_calloc_fn_t)(size_t n, size_t size);
typedef void (*sys_free_fn_t)(void * ptr);

#define GBMEM_MALLOC(size) (gobj_malloc_func())(size)

#define GBMEM_FREE(ptr)             \
    if((ptr)) {                     \
        (gobj_free_func())((void *)(ptr));  \
        (ptr) = 0;                  \
    }

#define GBMEM_STRDUP gobj_strdup
#define GBMEM_STRNDUP gobj_strndup

#define GBMEM_REALLOC(ptr, size) (gobj_realloc_func())((ptr), (size))

/**************************************************************
 *       Prototypes
 **************************************************************/
PUBLIC int gbmem_setup( /* If you don't use the defaults, call this before gobj_start_up */
    size_t mem_max_block,          /* largest memory block, default 16M */
    size_t mem_max_system_memory,  /* maximum system memory, default 64M */
    BOOL   use_own_system_memory,  /* Use internal memory manager */
    // Below parameters are used only in internal memory manager:
    size_t mem_min_block,          /* smaller memory block, default 512 */
    size_t mem_superblock          /* superblock, default 16M */
);

PUBLIC void gbmem_shutdown(void);

PUBLIC int gbmem_set_allocators(
    sys_malloc_fn_t malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t calloc_func,
    sys_free_fn_t free_func
);
PUBLIC int gbmem_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
);

PUBLIC sys_malloc_fn_t gobj_malloc_func(void);
PUBLIC sys_realloc_fn_t gobj_realloc_func(void);
PUBLIC sys_calloc_fn_t gobj_calloc_func(void);
PUBLIC sys_free_fn_t gobj_free_func(void);
PUBLIC char *gobj_strndup(const char *str, size_t size);
PUBLIC char *gobj_strdup(const char *string);
PUBLIC size_t gobj_get_maximum_block(void);

PUBLIC void print_track_mem(void);
PUBLIC void set_memory_check_list(unsigned long *memory_check_list);


#ifdef __cplusplus
}
#endif
