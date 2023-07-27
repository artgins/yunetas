/***********************************************************************
 *              gbuffer.
 *
 *              Buffer growable and overflowable
 *
 *              Copyright (c) 2013-2023 Niyamaka.
 *              All Rights Reserved.
 ***********************************************************************/

#pragma once

#include <inttypes.h>
#include "gobj.h"
#include "helpers.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/

/*---------------------------------*
 *      GBuffer functions
 *---------------------------------*/
typedef struct gbuffer_s {
    DL_ITEM_FIELDS

    size_t refcount;            /* to delete by reference counter */

    char *label;                /* like user_data */
    size_t mark;                /* like user_data */

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
PUBLIC void gbuffer_incref(gbuffer_t *gbuf);
PUBLIC void gbuffer_decref(gbuffer_t *gbuf);

/*
 *  READING
 */
PUBLIC void *gbuffer_cur_rd_pointer(gbuffer_t *gbuf);  /* Return current reading pointer */
PUBLIC void gbuffer_reset_rd(gbuffer_t *gbuf);        /* reset read pointer */

PUBLIC int gbuffer_set_rd_offset(gbuffer_t *gbuf, size_t position);
PUBLIC int gbuffer_ungetc(gbuffer_t *gbuf, char c);
PUBLIC size_t gbuffer_get_rd_offset(gbuffer_t *gbuf);
/*
 * Pop 'len' bytes, return the pointer.
 * If there is no `len` bytes of data to pop, return 0, and no data is popped.
 */
PUBLIC void *gbuffer_get(gbuffer_t *gbuf, size_t len);
PUBLIC char gbuffer_getchar(gbuffer_t *gbuf);   /* pop one bytes */
PUBLIC size_t gbuffer_chunk(gbuffer_t *gbuf);   /* return the chunk of data available */
PUBLIC char *gbuffer_getline(gbuffer_t *gbuf, char separator);

/*
 *  WRITING
 */
PUBLIC void *gbuffer_cur_wr_pointer(gbuffer_t *gbuf);  /* Return current writing pointer */
PUBLIC void gbuffer_reset_wr(gbuffer_t *gbuf);     /* reset write pointer (empty write/read data) */
PUBLIC int gbuffer_set_wr(gbuffer_t *gbuf, size_t offset);
PUBLIC size_t gbuffer_append(gbuffer_t *gbuf, void *data, size_t len);   /* return bytes written */
PUBLIC size_t gbuffer_append_string(gbuffer_t *gbuf, const char *s);
PUBLIC size_t gbuffer_append_char(gbuffer_t *gbuf, char c);              /* return bytes written */
PUBLIC int gbuffer_append_gbuf(gbuffer_t *dst, gbuffer_t *src);
PUBLIC int gbuffer_printf(gbuffer_t *gbuf, const char *format, ...) JANSSON_ATTRS((format(printf, 2, 3)));
PUBLIC int gbuffer_vprintf(gbuffer_t *gbuf, const char *format, va_list ap) JANSSON_ATTRS((format(printf, 2, 0)));;

/*
 *  Util
 */
PUBLIC void *gbuffer_head_pointer(gbuffer_t *gbuf);  /* Return pointer to first position of data */
PUBLIC void gbuffer_clear(gbuffer_t *gbuf);  // Reset write/read pointers

PUBLIC size_t gbuffer_leftbytes(gbuffer_t *gbuf);   /* nº bytes remain of reading */
PUBLIC size_t gbuffer_totalbytes(gbuffer_t *gbuf);  /* total written bytes */
PUBLIC size_t gbuffer_freebytes(gbuffer_t *gbuf);   /* free space */

PUBLIC int gbuffer_setlabel(gbuffer_t *gbuf, const char *label);
PUBLIC char *gbuffer_getlabel(gbuffer_t *gbuf);
PUBLIC void gbuffer_setmark(gbuffer_t *gbuf, size_t mark);
PUBLIC size_t gbuffer_getmark(gbuffer_t *gbuf);

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
 */
PUBLIC gbuffer_t *gbuffer_string_to_base64(const char* src, size_t len); // base64 without newlines

/*
 *  Decode from base64
 */
PUBLIC gbuffer_t *gbuffer_base64_to_string(const char* base64, size_t base64_len);

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
