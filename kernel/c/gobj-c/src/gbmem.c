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
 *              Copyright (c) 2024-2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>

#include "ansi_escape_codes.h"  /* used by ESP */
#include "gtypes.h"
#include "dl_list.h"
#include "gbmem.h"
#include "glogger.h"

/***************************************************************
 *              Data
 ***************************************************************/
#ifdef CONFIG_DEBUG_TRACK_MEMORY
    PRIVATE size_t mem_ref = 0;
    PRIVATE dl_list_t dl_busy_mem = {0};

    typedef struct {
        DL_ITEM_FIELDS
        size_t size;
        size_t ref;
        void *p;
    } track_mem_t;

    unsigned long *memory_check_list = 0;

    /*
     *  The catch window (YUNETA_TRACK_MEM), see read_track_window()
     */
    PRIVATE size_t track_ref_min = 0;
    PRIVATE size_t track_ref_max = 0;
    PRIVATE size_t track_sizes[8] = {0};
    PRIVATE int track_sizes_len = 0;

#define TRACK_MEM sizeof(track_mem_t)
#else
    // typedef struct {
    //     size_t size;
    // } track_mem_t;
#define TRACK_MEM 0
#endif

PRIVATE void *_mem_malloc(size_t size);
PRIVATE void _mem_free(void *p);
PRIVATE void *_mem_realloc(void *p, size_t new_size);
PRIVATE void *_mem_calloc(size_t n, size_t size);

PRIVATE sys_malloc_fn_t sys_malloc_fn = _mem_malloc;
PRIVATE sys_realloc_fn_t sys_realloc_fn = _mem_realloc;
PRIVATE sys_calloc_fn_t sys_calloc_fn = _mem_calloc;
PRIVATE sys_free_fn_t sys_free_fn = _mem_free;

PRIVATE size_t __max_block__ = 16*1024L*1024L; /* largest memory block, default for no-using apps*/
PRIVATE size_t __max_system_memory__ = 64*1024L*1024L;   /* maximum core memory, default for no-using apps */

#ifdef CONFIG_DEBUG_TRACK_MEMORY
PRIVATE size_t __cur_system_memory__ = 0;   /* current system memory */
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
            "msgset",       "%s", MSGSET_PARAMETER,
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
    return __max_block__ - TRACK_MEM;
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
#ifdef CONFIG_DEBUG_TRACK_MEMORY
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
#ifdef CONFIG_DEBUG_TRACK_MEMORY
    memory_check_list = memory_check_list_;
#endif
}

/***********************************************************************
 *      Print track memory
 ***********************************************************************/
PUBLIC void print_track_mem(void)
{
#ifdef CONFIG_DEBUG_TRACK_MEMORY
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

    /*
     *  WHERE THE LIST ENDS, taken BEFORE anything is logged.
     *
     *  Logging allocates, and every allocation is APPENDED to this same
     *  list, so a walk that runs to the end can walk into what the walk
     *  itself is writing and report it as leaked -- without ever ending,
     *  if a handler holds one block per line. The report is what was busy
     *  at THIS instant and nothing the report itself allocates.
     *
     *  Measured 2026-09-20 while chasing db_history_ce: this guard did NOT
     *  change that yuno's count (1207 before and after), so what it reports
     *  there was already busy when the walk began. The guard closes the
     *  hazard, it does not explain that case.
     */
    track_mem_t *last = dl_last(&dl_busy_mem);

    gobj_log_error(0, 0,
        "function",         "%s", __FUNCTION__,
        "msgset",           "%s", MSGSET_STATISTICS,
        "msg",              "%s", "print_track_mem(): system memory not free",
        "program",          "%s", cmdline,
        "window_min",       "%lu", (unsigned long)track_ref_min,
        "window_max",       "%lu", (unsigned long)track_ref_max,
        NULL
    );

    /*
     *  What the blocks HOLD, with YUNETA_TRACK_MEM_DUMP.
     *
     *  Printable bytes and not a cast to json_t: a block of a given size is
     *  not necessarily the jansson struct it looks like, and reading a type
     *  field out of a guess is how a report crashes instead of arriving. The
     *  blocks that name the leak are the text ones anyway -- a leaked string
     *  carries its characters, and they say which field of which record it
     *  was.
     */
    BOOL dump_bytes = getenv("YUNETA_TRACK_MEM_DUMP")? TRUE: FALSE;

    track_mem_t *track_mem = dl_first(&dl_busy_mem);
    while(track_mem) {
        track_mem_t *next = (track_mem == last)? NULL : dl_next(track_mem);

        if(dump_bytes) {
            char bytes[65] = {0};
            if(track_mem->p && track_mem->size > TRACK_MEM) {
                const uint8_t *pp = (const uint8_t *)track_mem->p;
                size_t len = track_mem->size - TRACK_MEM;
                if(len > sizeof(bytes) - 1) {
                    len = sizeof(bytes) - 1;
                }
                for(size_t ii = 0; ii < len; ii++) {
                    bytes[ii] = (pp[ii] < ' ' || pp[ii] > 0x7f)? '.': (char)pp[ii];
                }
                bytes[len] = 0;
            }

            gobj_log_debug(0,0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TRACK_MEM,
                "msg",          "%s", "mem-not-free",
                "ref",          "%lu", (unsigned long)track_mem->ref,
                "size",         "%lu", (unsigned long)track_mem->size,
                "p",            "%lu", (unsigned long)track_mem->p,
                "bytes",        "%s", bytes,
                NULL
            );
        } else {
            gobj_log_debug(0,0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_TRACK_MEM,
                "msg",          "%s", "mem-not-free",
                "ref",          "%lu", (unsigned long)track_mem->ref,
                "size",         "%lu", (unsigned long)track_mem->size,
                "p",            "%lu", (unsigned long)track_mem->p,
                NULL
            );
        }

        track_mem = next;
    }
#endif
}

/***********************************************************************
 *      The catch window: which allocations to photograph while they happen
 *
 *  `memory_check_list` needs the exact ref or the exact size. A ref cannot
 *  be prepared in advance -- it moves a few hundred between two runs of the
 *  same yuno -- and a size alone catches tens of thousands of blocks in a
 *  yuno that starts by loading a database. A WINDOW of refs, narrowed by a
 *  handful of sizes, is what a second run can aim at what the first run
 *  reported.
 *
 *      YUNETA_TRACK_MEM=<ref_min>-<ref_max>[:<size>,<size>,...]
 *
 *  Read once, on the first allocation: the logger has no handlers yet, so
 *  nothing is said here. print_track_mem() prints the window it used.
 ***********************************************************************/
#ifdef CONFIG_DEBUG_TRACK_MEMORY
PRIVATE void read_track_window(void)
{
    const char *value = getenv("YUNETA_TRACK_MEM");
    if(!value) {
        return;
    }

    char *end = 0;
    size_t min = (size_t)strtoul(value, &end, 10);
    if(!end || *end != '-') {
        return;     // Malformed: print_track_mem() reports the window as off
    }
    size_t max = (size_t)strtoul(end + 1, &end, 10);
    if(max < min) {
        return;     // Idem
    }
    track_ref_min = min;
    track_ref_max = max;

    if(end && *end == ':') {
        const char *p = end + 1;
        while(*p && track_sizes_len < (int)ARRAY_SIZE(track_sizes)) {
            track_sizes[track_sizes_len++] = (size_t)strtoul(p, 0, 10);
            p = strchr(p, ',');
            if(!p) {
                break;
            }
            p++;
        }
    }
}

/***********************************************************************
 *
 ***********************************************************************/
PRIVATE void check_failed_list(track_mem_t *track_mem)
{
    /*
     *  The catch window. The re-entrancy guard is not optional: the log
     *  below allocates, and its own allocations fall inside the window too.
     */
    static BOOL window_read = FALSE;
    static BOOL inside = FALSE;

    if(!window_read) {
        window_read = TRUE;
        read_track_window();
    }

    if(track_ref_max && !inside &&
        track_mem->ref >= track_ref_min && track_mem->ref <= track_ref_max
    ) {
        BOOL take = (track_sizes_len == 0)? TRUE: FALSE;
        for(int ii = 0; ii < track_sizes_len; ii++) {
            if(track_sizes[ii] == track_mem->size) {
                take = TRUE;
                break;
            }
        }
        if(take) {
            inside = TRUE;
            gobj_log_debug(0, LOG_OPT_TRACE_STACK,
                "msgset",       "%s", MSGSET_STATISTICS,
                "msg",          "%s", "mem-in-window",
                "ref",          "%lu", (unsigned long)track_mem->ref,
                "size",         "%lu", (unsigned long)track_mem->size,
                NULL
            );
            inside = FALSE;
        }
    }

    for(int xx=0; memory_check_list && memory_check_list[xx]!=0; xx++) {
        if(memory_check_list[xx] == track_mem->ref) {
            gobj_log_debug(0, LOG_OPT_TRACE_STACK,
                "msgset",       "%s", MSGSET_STATISTICS,
                "msg",          "%s", "mem-not-free by ref",
                "ref",          "%ul", (unsigned long)track_mem->ref,
                "p",            "%ul", (unsigned long)track_mem->p,
                NULL
            );
        } else if(memory_check_list[xx] == track_mem->size) {
            /*  With the REF: a size catches thousands of allocations in a
             *  yuno that starts by loading a database (95.648 of them for
             *  one size, measured), and the ref is what tells which of
             *  them is the one print_track_mem() reported.  */
            gobj_log_debug(0, LOG_OPT_TRACE_STACK,
                "msgset",       "%s", MSGSET_STATISTICS,
                "msg",          "%s", "mem-not-free by size",
                "ref",          "%lu", (unsigned long)track_mem->ref,
                "size",         "%lu", (unsigned long)track_mem->size,
                "p",            "%lu", (unsigned long)track_mem->p,
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
#ifdef CONFIG_DEBUG_TRACK_MEMORY
    size_t extra = TRACK_MEM;
    size += extra;
#endif
    if(size > __max_block__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "SIZE GREATER THAN MAX_BLOCK",
            "size",         "%ld", (long)size,
            "max_block",    "%d", (int)__max_block__,
            NULL
        );
        return NULL;
    }

#ifdef CONFIG_DEBUG_TRACK_MEMORY
    __cur_system_memory__ += size;

    if(__cur_system_memory__ > __max_system_memory__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY,
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
            "msgset",               "%s", MSGSET_MEMORY,
            "msg",                  "%s", "NO MEMORY calloc() failed",
            "size",                 "%ld", (long)size,
            NULL
        );
    }

#ifdef CONFIG_DEBUG_TRACK_MEMORY
    track_mem_t *pm_ = (track_mem_t*)pm;
    pm_->size = size;
    pm_->ref = ++mem_ref;
    dl_add(&dl_busy_mem, pm_);

    check_failed_list(pm_);
    pm += extra;
    pm_->p = pm;
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
#ifdef CONFIG_DEBUG_TRACK_MEMORY
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

#ifdef CONFIG_DEBUG_TRACK_MEMORY
    size_t extra = TRACK_MEM;
    new_size += extra;
#endif

    /*
     *  Refused BEFORE the block leaves the tracking: the caller keeps the
     *  old block, valid, and frees it later as a tracked one
     */
    if(new_size > __max_block__) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "SIZE GREATER THAN MAX_BLOCK",
            "size",         "%ld", (long)new_size,
            "max_block",    "%d", (int)__max_block__,
            NULL
        );
        return NULL;
    }

#ifdef CONFIG_DEBUG_TRACK_MEMORY
    char *pm = p;
    pm -= extra;

    track_mem_t *pm_ = (track_mem_t*)pm;
    size_t size = pm_->size;

    dl_delete(&dl_busy_mem, pm_, 0);
    __cur_system_memory__ -= size;

    __cur_system_memory__ += new_size;
    if(__cur_system_memory__ > __max_system_memory__) {
        gobj_log_critical(0, LOG_OPT_ABORT,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY,
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
            "msgset",               "%s", MSGSET_MEMORY,
            "msg",                  "%s", "NO MEMORY realloc() failed",
            "new_size",             "%ld", (long)new_size,
            NULL
        );
    }
#ifdef CONFIG_DEBUG_TRACK_MEMORY
    pm = pm__;

    pm_ = (track_mem_t*)pm;
    pm_->size = new_size;

    pm_->ref = ++mem_ref;
    dl_add(&dl_busy_mem, pm_);
    pm += extra;
    pm_->p = pm;
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
