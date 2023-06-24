/****************************************************************************
 *          ISTREAM.H
 *          Input stream
 *
 *          Mixin: process-data & emit events
 *          Copyright (c) 2013-2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"
#include "gbuffer.h"

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Constants
 *********************************************************************/
typedef void *istream;

#define ISTREAM_CREATE(var, gobj, data_size, max_size)                  \
    if(var) {                                                           \
        gobj_log_error((gobj), LOG_OPT_TRACE_STACK,                     \
            "function",     "%s", __FUNCTION__,                         \
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,                \
            "msg",          "%s", "istream ALREADY exists! Destroyed",  \
            NULL                                                        \
        );                                                              \
        istream_destroy(var);                                           \
    }                                                                   \
    (var) = istream_create((gobj), (data_size), (max_size));

#define ISTREAM_DESTROY(ptr)    \
    if(ptr) {                   \
        istream_destroy(ptr);   \
    }                           \
    (ptr) = 0;

/*********************************************************************
 *      Prototypes
 *********************************************************************/
PUBLIC istream istream_create(
    hgobj gobj,
    size_t data_size,
    size_t max_size
);
PUBLIC void istream_destroy(istream istream);
PUBLIC int istream_read_until_num_bytes(
    istream istream,
    size_t num_bytes,
    const char *event
);
PUBLIC int istream_read_until_delimiter(
    istream istream,
    const char *delimiter,
    size_t delimiter_size,
    const char *event
);
PUBLIC size_t istream_consume(istream istream, char *bf, size_t len);
PUBLIC char *istream_cur_rd_pointer(istream istream);
PUBLIC size_t istream_length(istream istream);
PUBLIC gbuffer_t *istream_get_gbuffer(istream istream);
PUBLIC gbuffer_t *istream_pop_gbuffer(istream istream);
PUBLIC int istream_new_gbuffer(istream istream, size_t data_size, size_t max_size);
PUBLIC char *istream_extract_matched_data(istream istream, size_t *len);
PUBLIC int istream_reset_wr(istream istream);
PUBLIC int istream_reset_rd(istream istream);
PUBLIC void istream_clear(istream istream); // reset wr/rd
PUBLIC BOOL istream_is_completed(istream istream);

#ifdef __cplusplus
}
#endif
