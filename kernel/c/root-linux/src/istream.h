/****************************************************************************
 *              istream.h
 *              Input stream
 *
 *              Mixin: process-data & emit events
 *              Copyright (c) 2013 Niyamaka.
 *              Copyright (c) 2025, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Prototypes
 *********************************************************************/
typedef void *istream_h;

PUBLIC istream_h istream_create(
    hgobj gobj,
    size_t data_size,
    size_t max_size
);

#define ISTREAM_CREATE(var, gobj, data_size, max_size)                  \
    if(var) {                                                           \
        gobj_log_error((gobj), LOG_OPT_TRACE_STACK,                     \
            "function",     "%s", __FUNCTION__,                         \
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,                \
            "msg",          "%s", "istream_h ALREADY exists! Destroyed",  \
            NULL                                                        \
        );                                                              \
        istream_destroy(var);                                           \
    }                                                                   \
    (var) = istream_create((gobj), (data_size), (max_size));


PUBLIC void istream_destroy(
    istream_h istream
);

#define ISTREAM_DESTROY(istream)    \
    if(istream) {                   \
        istream_destroy(istream);   \
    }                               \
    (istream) = 0;


PUBLIC int istream_read_until_num_bytes(
    istream_h istream,
    size_t num_bytes,
    gobj_event_t event
);
PUBLIC int istream_read_until_delimiter(
    istream_h istream,
    const char *delimiter,
    size_t delimiter_size,
    gobj_event_t event
);
PUBLIC size_t istream_consume(
    istream_h istream,
    char *bf,
    size_t len
);
PUBLIC char *istream_cur_rd_pointer(
    istream_h istream
);
PUBLIC size_t istream_length(
    istream_h istream
);
PUBLIC gbuffer_t *istream_get_gbuffer(
    istream_h istream
);
PUBLIC gbuffer_t *istream_pop_gbuffer(
    istream_h istream
);
PUBLIC int istream_new_gbuffer(
    istream_h istream,
    size_t data_size,
    size_t max_size
);
PUBLIC char *istream_extract_matched_data(
    istream_h istream,
    size_t *len
);
PUBLIC int istream_reset_wr(
    istream_h istream
);
PUBLIC int istream_reset_rd(
    istream_h istream
);
PUBLIC void istream_clear(// reset wr/rd
    istream_h istream
);
PUBLIC BOOL istream_is_completed(
    istream_h istream
);


#ifdef __cplusplus
}
#endif
