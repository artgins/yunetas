/****************************************************************************
 *              gbuffer.h
 *
 *              Buffer growable and overflowable
 *
 *              Copyright (c) 2013 Niyamaka.
 *              Copyright (c) 2024, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <string.h>
#include <netdb.h>  // need it by struct sockaddr

#include "gtypes.h"
#include "dl_list.h"
#include "glogger.h"

#ifdef __cplusplus
extern "C"{
#endif

/**************************************************************
 *       Constants
 **************************************************************/

/***************************************************************
 *       Structures
 ***************************************************************/
typedef struct gbuffer_s {
    DL_ITEM_FIELDS

    size_t refcount;            /* to delete by reference counter */

    char *label;                /* like user_data */
    size_t mark;                /* like user_data */
    struct sockaddr addr;       /* like user_data, mainly to use in udp */

    size_t data_size;           /* nº bytes allocated for data */
    size_t max_memory_size;     /* maximum size in memory */

    /*
     *  Con tail controlo la entrada de bytes en el packet.
     *  El número total de bytes en el packet será tail.
     */
    size_t tail;    /* write pointer */
    size_t curp;    /* read pointer */

    /*
     *  Data dynamically allocated
     *  In file_mode this buffer only is for read from file.
     */
    char *data;
} gbuffer_t;

/**************************************************************
 *       Prototypes
 **************************************************************/

#define GBUFFER_DECREF(ptr)    \
    if(ptr) {               \
        gbuffer_decref(ptr);   \
        (ptr) = 0;          \
    }

#define GBUFFER_INCREF(ptr)    \
    if(ptr) {               \
        gbuffer_incref(ptr);   \
    }


PUBLIC gbuffer_t *gbuffer_create(
    size_t data_size,
size_t max_memory_size
);
PUBLIC void gbuffer_remove(gbuffer_t *gbuf); /* WARNING do not call gbuffer_remove(), call gbuffer_decref() */

static inline gbuffer_t *gbuffer_incref(gbuffer_t *gbuf) /* Incr ref */
{
    if(!gbuf || gbuf->refcount <= 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "BAD gbuf_incref()",
            NULL
        );
        return NULL;
    }
    ++(gbuf->refcount);
    return gbuf;
}

static inline void gbuffer_decref(gbuffer_t *gbuf) /* Decr ref */
{
    if(!gbuf || gbuf->refcount <= 0) {
        gobj_log_error(0, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "BAD gbuf_decref()",
            NULL
        );
        return;
    }
    --(gbuf->refcount);
    if(gbuf->refcount == 0)
        gbuffer_remove(gbuf);
}

/*
 *  READING
 */
static inline void * gbuffer_cur_rd_pointer(gbuffer_t *gbuf) /* Return current reading pointer */
{
    return gbuf->data + gbuf->curp;
}

static inline void gbuffer_reset_rd(gbuffer_t *gbuf) /* reset read pointer */
{
    gbuf->curp = 0;
}

PUBLIC int gbuffer_set_rd_offset(gbuffer_t *gbuf, size_t position);

static inline int gbuffer_ungetc(gbuffer_t *gbuf, char c) /* Unget char */
{
    if(gbuf->curp > 0) {
        gbuf->curp--;

        *(gbuf->data + gbuf->curp) = c;
    }

    return 0;
}

static inline size_t gbuffer_get_rd_offset(gbuffer_t *gbuf) /* Get current output pointer */
{
    return gbuf->curp;
}


/*
 * Pop 'len' bytes, return the pointer.
 * If there is no `len` bytes of data to pop, return 0, and no data is popped.
 */
static inline void * gbuffer_get(gbuffer_t *gbuf, size_t len)
{
    if(len <= 0) {
        return NULL;
    }

    size_t rest = gbuf->tail - gbuf->curp; // gbuffer_leftbytes(gbuf);

    if(len > rest) {
        return NULL;
    }
    char *p = gbuf->data + gbuf->curp;
    gbuf->curp += len;     /* remove bytes from gbuf */
    return p;
}

static inline char gbuffer_getchar(gbuffer_t *gbuf) /* pop one byte */
{
    char *p = gbuffer_get(gbuf, 1);
    if(p) {
        return *p;
    } else {
        return 0;
    }
}

static inline size_t gbuffer_chunk(gbuffer_t *gbuf) /* return the chunk of data available */
{
    size_t ln = gbuf->tail - gbuf->curp; /* gbuffer_leftbytes() */
    size_t chunk_size = MIN(gbuf->data_size, ln);

    return chunk_size;
}

PUBLIC char *gbuffer_getline(gbuffer_t *gbuf, char separator);

/*
 *  WRITING
 */
static inline void *gbuffer_cur_wr_pointer(gbuffer_t *gbuf) /* Return current writing pointer */
{
    return gbuf->data + gbuf->tail;
}

static inline void gbuffer_reset_wr(gbuffer_t *gbuf) /* reset write pointer (empty write/read data) */
{
    gbuf->tail = 0;
    gbuf->curp = 0;
    *(gbuf->data + gbuf->tail) = 0; /* Put final null */
}

PUBLIC int gbuffer_set_wr(gbuffer_t *gbuf, size_t offset);
PUBLIC size_t gbuffer_append(gbuffer_t *gbuf, void *data, size_t len);   /* return bytes written */

static inline size_t gbuffer_append_string(gbuffer_t *gbuf, const char *s) /* return bytes written */
{
    return gbuffer_append(gbuf, (void *)s, strlen(s));
}
static inline size_t gbuffer_append_char(gbuffer_t *gbuf, char c) /* return bytes written */
{
    return gbuffer_append(gbuf, &c, 1);
}

PUBLIC size_t gbuffer_append_json( // Old json_append2gbuf
    gbuffer_t *gbuf,
    json_t *jn  // owned
);

PUBLIC int gbuffer_append_gbuf(gbuffer_t *dst, gbuffer_t *src);

PUBLIC int gbuffer_printf(gbuffer_t *gbuf, const char *format, ...) JANSSON_ATTRS((format(printf, 2, 3)));
PUBLIC int gbuffer_vprintf(gbuffer_t *gbuf, const char *format, va_list ap) JANSSON_ATTRS((format(printf, 2, 0)));;

/*
 *  Util
 */
static inline void *gbuffer_head_pointer(gbuffer_t *gbuf)
{
    return gbuf->data; /* Return pointer to first position of data */
}

static inline void gbuffer_clear(gbuffer_t *gbuf)
{
    /* Reset write/read pointers */
    gbuffer_reset_wr(gbuf);
    gbuffer_reset_rd(gbuf);
}

static inline size_t gbuffer_leftbytes(gbuffer_t *gbuf)
{
    /* nº bytes remain of reading */
    return gbuf->tail - gbuf->curp;
}

static inline size_t gbuffer_totalbytes(gbuffer_t *gbuf)
{
    /* total written bytes */
    return gbuf->tail;
}

static inline size_t gbuffer_freebytes(gbuffer_t *gbuf)
{
    /* free space */
    return gbuf->data_size - gbuf->tail;
}

PUBLIC int gbuffer_setlabel(gbuffer_t *gbuf, const char *label);

static inline char *gbuffer_getlabel(gbuffer_t *gbuf)
{
    /* Get label */
    return gbuf->label;
}

static inline void gbuffer_setmark(gbuffer_t *gbuf, size_t mark)
{
    /* Set mark */
    gbuf->mark = mark;
}

static inline size_t gbuffer_getmark(gbuffer_t *gbuf)
{
    /* Get mark */
    return gbuf->mark;
}

static inline void gbuffer_setaddr(gbuffer_t *gbuf, struct sockaddr *addr)
{
    gbuf->addr = *addr;
}

static inline struct sockaddr *gbuffer_getaddr(gbuffer_t *gbuf)
{
    return &gbuf->addr;
}

PUBLIC int gbuf2file(
    hgobj gobj,
    gbuffer_t *gbuf,  // WARNING own
    const char *path,
    int permission,
    BOOL overwrite
);

PUBLIC json_t* gbuffer_serialize(
    hgobj gobj,
    gbuffer_t *gbuf // not owned
);
PUBLIC gbuffer_t *gbuffer_deserialize(
    hgobj gobj,
    const json_t *jn  // not owned
);

/*
 *  Encode to base64
 *  Old gbuf_string2base64
 */
PUBLIC gbuffer_t *gbuffer_string_to_base64(const char* src, size_t len); // base64 without newlines
PUBLIC gbuffer_t *gbuffer_file2base64(const char *path);

/*
 *  Decode from base64
 *  Old gbuf_decodebase64string(), gbuf_decodebase64stringn()
 */
PUBLIC gbuffer_t *gbuffer_base64_to_string(const char* base64, size_t base64_len);

/*
 *  Return new gbuffer using gbuffer_string_to_base64() to encode string in gbuf_input
 */
PUBLIC gbuffer_t *gbuffer_encode_base64( // return new gbuffer using gbuffer_string_to_base64()
    gbuffer_t *gbuf_input  // decref
);

/*
 *  String to a new gbuffer
 */
PUBLIC gbuffer_t *str2gbuf(
    const char *fmt,
    ...
)  JANSSON_ATTRS((format(printf, 1, 2)));

/*
 *  Json to gbuffer
 */
PUBLIC gbuffer_t *json2gbuf(
    gbuffer_t *gbuf,
    json_t *jn, // owned
    size_t flags
);
/*
 *  Json from gbuffer
 */
PUBLIC json_t *gbuf2json(
    gbuffer_t *gbuf,  // WARNING gbuf own and data consumed
    int verbose     // 1 log, 2 log+dump
);

PUBLIC void gobj_trace_dump_gbuf(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *fmt,
    ...
)  JANSSON_ATTRS((format(printf, 3, 4)));
PUBLIC void gobj_trace_dump_full_gbuf(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *fmt,
    ...
)  JANSSON_ATTRS((format(printf, 3, 4)));

#ifdef __cplusplus
}
#endif
