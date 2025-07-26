/****************************************************************************
 *              gbmem.c
 *
 *              Block Memory Core
 *
 *
 * TODO find out some good memory manager
 *
 *  Originally from yuneta V6:
 *  Code inspired in zmalloc.c (MAWK)
 *  copyright 1991,1993, Michael D. Brennan
 *  Mawk is distributed without warranty under the terms of
 *  the GNU General Public License, version 2, 1991.
 *
 *   ZBLOCKS of sizes 1, 2, 3, ... 128
 *   are stored on the linked linear lists in
 *   pool[0], pool[1], pool[2],..., pool[127]
 *
 *   Para minimum size of 128 y pool size de 32 (pool[32]) ->
 *        sizes 128, 256, 384, 512, ... 2048,...,4096
 *   Para minimum size of 16 y pool size de 256 (pool[256]) ->
 *        sizes 16, 32, 48, 64,... 2048,...,4096
 *
 *
 *              Copyright (c) 1996-2023 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#include "gtypes.h"
#include "dl_list.h"
#include "gbmem.h"
#include "glogger.h"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE void *_mem_malloc(size_t size);
PRIVATE void _mem_free(void *p);
PRIVATE void *_mem_realloc(void *p, size_t new_size);
PRIVATE void *_mem_calloc(size_t n, size_t size);

PRIVATE sys_malloc_fn_t sys_malloc_fn = _mem_malloc;
PRIVATE sys_realloc_fn_t sys_realloc_fn = _mem_realloc;
PRIVATE sys_calloc_fn_t sys_calloc_fn = _mem_calloc;
PRIVATE sys_free_fn_t sys_free_fn = _mem_free;

PRIVATE size_t __max_block__ = 16*1024L*1024L;     /* largest memory block, default for no-using apps*/
PRIVATE size_t __max_system_memory__ = 64*1024L*1024L;   /* maximum core memory, default for no-using apps */

#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
PRIVATE size_t __cur_system_memory__ = 0;   /* current system memory */
#endif

#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    PRIVATE size_t mem_ref = 0;
    PRIVATE dl_list_t dl_busy_mem = {0};

    typedef struct {
        DL_ITEM_FIELDS
        size_t size;
        size_t ref;
    } track_mem_t;

    unsigned long *memory_check_list = 0;
#define TRACK_MEM sizeof(track_mem_t)
#else
    // typedef struct {
    //     size_t size;
    // } track_mem_t;
#endif

/***************************************************************************
 *  Initialize memory manager
 ***************************************************************************/
PUBLIC int gbmem_setup( /* If you don't use the defaults, call this before gobj_start_up */
    size_t                      mem_max_block,          /* largest memory block, default 16M */
    size_t                      mem_max_system_memory,  /* maximum system memory, default 64M */
    BOOL                        use_own_system_memory,  /* Use internal memory manager */
    // Below parameters are used only in internal memory manager:
    size_t                      mem_min_block,          /* smaller memory block, default 512 */
    size_t                      mem_superblock          /* superblock, default 16M */
) {
    /*
     *  TODO: To use in internal memory manager
     */
    (void)use_own_system_memory;
    (void)mem_min_block;
    (void)mem_superblock;

    if(mem_max_block) {
        __max_block__ = mem_max_block;
    }
    if(mem_max_system_memory) {
        __max_system_memory__ = mem_max_system_memory;
    }

    return 0;
}

/***************************************************************************
 *     Close memory manager
 ***************************************************************************/
PUBLIC void gbmem_shutdown(void)
{

}

/***************************************************************************
 *     Set memory functions
 ***************************************************************************/
PUBLIC int gbmem_set_allocators(
    sys_malloc_fn_t malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t calloc_func,
    sys_free_fn_t free_func
) {
    sys_malloc_fn = malloc_func;
    sys_realloc_fn = realloc_func;
    sys_calloc_fn = calloc_func;
    sys_free_fn = free_func;

    return 0;
}

/***************************************************************************
 *     Get memory functions
 ***************************************************************************/
PUBLIC int gbmem_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
) {
    if(malloc_func) {
        *malloc_func = sys_malloc_fn;
    }
    if(realloc_func) {
        *realloc_func = sys_realloc_fn;
    }
    if(calloc_func) {
        *calloc_func = sys_calloc_fn;
    }
    if(free_func) {
        *free_func = sys_free_fn;
    }

    return 0;
}

/***********************************************************************
 *      Memory functions
 ***********************************************************************/
PUBLIC void *gbmem_malloc(size_t size)
{
    return sys_malloc_fn(size);
}

/***********************************************************************
 *      Memory functions
 ***********************************************************************/
PUBLIC void gbmem_free(void *ptr)
{
    sys_free_fn(ptr);
}

/***********************************************************************
 *      Memory functions
 ***********************************************************************/
PUBLIC void *gbmem_realloc(void *ptr, size_t size)
{
    return sys_realloc_fn(ptr, size);
}

/***********************************************************************
 *      Memory functions
 ***********************************************************************/
PUBLIC void *gbmem_calloc(size_t n, size_t size)
{
    return sys_calloc_fn(n, size);
}

/***************************************************************************
 *     duplicate a substring
 ***************************************************************************/
PUBLIC char *gbmem_strndup(const char *str, size_t size)
{
    char *s;

    /*-----------------------------------------*
     *     Check null string
     *-----------------------------------------*/
    if(!str) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "str is NULL",
            NULL
        );
        return NULL;
    }

    /*-----------------------------------------*
     *     Alloca memoria
     *-----------------------------------------*/
    s = (char *)sys_malloc_fn(size+1);
    if(!s) {
        return NULL;
    }

    /*-----------------------------------------*
     *     Copy the substring
     *-----------------------------------------*/
    memmove(s, str, size);

    return s;
}

/***************************************************************************
 *     Duplica un string
 ***************************************************************************/
PUBLIC char *gbmem_strdup(const char *string)
{
    if(!string) {
        return NULL;
    }
    return gbmem_strndup(string, strlen(string));
}

/*************************************************************************
 *  Return the maximum memory that you can get
 *************************************************************************/
PUBLIC size_t gbmem_get_maximum_block(void)
{
    return __max_block__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC size_t get_max_system_memory(void)
{
    return __max_system_memory__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC size_t get_cur_system_memory(void)
{
#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    return __cur_system_memory__;
#else
    return 0;
#endif
}

/***********************************************************************
 *      Set mem ref list to check
 ***********************************************************************/
PUBLIC void set_memory_check_list(unsigned long *memory_check_list_)
{
#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    memory_check_list = memory_check_list_;
#endif
}

/***********************************************************************
 *      Print track memory
 ***********************************************************************/
PUBLIC void print_track_mem(void)
{
#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    if(!__cur_system_memory__) {
        return;
    }

    char cmdline[4096] = {0};
    FILE *f = fopen("/proc/self/cmdline", "r");
    if(f) {
        size_t n = fread(cmdline, 1, sizeof(cmdline) - 1, f);
        fclose(f);
        cmdline[n] = 0;
    }

    gobj_log_error(0, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_STATISTICS,
        "msg",              "%s", "print_track_mem(): system memory not free",
        "program",          "%s", cmdline,
        NULL
    );

    track_mem_t *track_mem = dl_first(&dl_busy_mem);
    while(track_mem) {
        gobj_log_debug(0,0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_TRACK_MEM,
            "msg",          "%s", "mem-not-free",
            "ref",          "%lu", (unsigned long)track_mem->ref,
            "size",         "%lu", (unsigned long)track_mem->size,
            NULL
        );

        track_mem = dl_next(track_mem);
    }
#endif
}

/***********************************************************************
 *
 ***********************************************************************/
#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
PRIVATE void check_failed_list(track_mem_t *track_mem)
{
    for(int xx=0; memory_check_list && memory_check_list[xx]!=0; xx++) {
        if(track_mem->ref  == memory_check_list[xx]) {
            gobj_log_debug(0, LOG_OPT_TRACE_STACK,
                "msgset",       "%s", MSGSET_STATISTICS,
                "msg",          "%s", "mem-not-free",
                "ref",          "%ul", (unsigned long)track_mem->ref,
                NULL
            );
        }
        if(xx > 5) {
            break;  // bit a bit please
        }
    }
}
#endif

/***********************************************************************
 *      Alloc memory
 ***********************************************************************/
PRIVATE void *_mem_malloc(size_t size)
{
#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    size_t extra = TRACK_MEM;
    size += extra;
#endif
    if(size > __max_block__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "SIZE GREATER THAN MAX_BLOCK",
            "size",         "%ld", (long)size,
            "max_block",    "%d", (int)__max_block__,
            NULL
        );
        return NULL;
    }

#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    __cur_system_memory__ += size;

    if(__cur_system_memory__ > __max_system_memory__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "REACHED MAX_SYSTEM_MEMORY",
            NULL
        );
    }
#endif

    char *pm = calloc(1, size);
    if(!pm) {
#ifdef ESP_PLATFORM
        #include <esp_system.h>
        printf(On_Red BWhite "ERROR NO MEMORY calloc() failed, size %d, HEAP free %d" Color_Off "\n", (int)size, (int)esp_get_free_heap_size());
#endif
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "NO MEMORY calloc() failed",
            "size",                 "%ld", (long)size,
            NULL
        );
    }

#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    track_mem_t *pm_ = (track_mem_t*)pm;
    pm_->size = size;
    pm_->ref = ++mem_ref;
    dl_add(&dl_busy_mem, pm_);

    check_failed_list(pm_);
    pm += extra;
#endif

    return pm;
}

/***********************************************************************
 *      Free memory
 ***********************************************************************/
PRIVATE void _mem_free(void *p)
{
    if(!p) {
        return; // El comportamiento como free() es que no salga error;
    }
#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    size_t extra = TRACK_MEM;

    char *pm = p;
    pm -= extra;

    track_mem_t *pm_ = (track_mem_t*)pm;
    size_t size = pm_->size;

    dl_delete(&dl_busy_mem, pm_, 0);
    __cur_system_memory__ -= size;
    memset(pm, 0, size);
    free(pm);
#else
    free(p);
#endif

}

/***************************************************************************
 *     ReAlloca memoria del core
 ***************************************************************************/
PRIVATE void *_mem_realloc(void *p, size_t new_size)
{
    /*---------------------------------*
     *  realloc admit p null
     *---------------------------------*/
    if(!p) {
        return _mem_malloc(new_size);
    }

#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    size_t extra = TRACK_MEM;
    new_size += extra;

    char *pm = p;
    pm -= extra;

    track_mem_t *pm_ = (track_mem_t*)pm;
    size_t size = pm_->size;

    dl_delete(&dl_busy_mem, pm_, 0);
    __cur_system_memory__ -= size;
#endif

    if(new_size > __max_block__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY_ERROR,
            "msg",          "%s", "SIZE GREATER THAN MAX_BLOCK",
            "size",         "%ld", (long)new_size,
            "max_block",    "%d", (int)__max_block__,
            NULL
        );
        return NULL;
    }

#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    __cur_system_memory__ += new_size;
    if(__cur_system_memory__ > __max_system_memory__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "REACHED MAX_SYSTEM_MEMORY",
            NULL
        );
    }

    char *pm__ = realloc(pm, new_size);
#else
    char *pm__ = realloc(p, new_size);
#endif
    if(!pm__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "NO MEMORY realloc() failed",
            "new_size",             "%ld", (long)new_size,
            NULL
        );
    }
#if defined(CONFIG_DEBUG_TRACK_MEMORY) && defined(CONFIG_BUILD_TYPE_DEBUG)
    pm = pm__;

    pm_ = (track_mem_t*)pm;
    pm_->size = new_size;

    pm_->ref = ++mem_ref;
    dl_add(&dl_busy_mem, pm_);
    pm += extra;
    return pm;
#else
    return pm__;
#endif

}

/***************************************************************************
 *     duplicate a substring
 ***************************************************************************/
PRIVATE void *_mem_calloc(size_t n, size_t size)
{
    size_t total = n * size;
    return _mem_malloc(total);
}
