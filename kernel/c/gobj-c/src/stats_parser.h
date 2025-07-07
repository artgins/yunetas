/****************************************************************************
 *          STATS_PARSER.H
 *
 *          Stats parser
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gtypes.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Prototypes
 **************************************************/
/**
    The default stats parser if mt_stats() is not implemented.
    Return build_stats() with build_command_response()
 */
PUBLIC json_t *stats_parser( // The default stats parser if mt_stats() is not implemented. Return with build_command_response()
    hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
);

/**
    Use by stats_parser(),
    Build stats from gobj's attributes with SDF_STATS|SDF_RSTATS|SDF_PSTATS flag.
    If `stats` == '__reset__' reset all SDF_RSTATS stats.
    It includes all gobj_bottom
*/
PUBLIC json_t *build_stats(
    hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
);

PUBLIC json_t *build_stats_response(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned, if null then not set
    json_t *jn_schema,  // owned, if null then not set
    json_t *jn_data     // owned, if null then not set
);


#ifdef __cplusplus
}
#endif
