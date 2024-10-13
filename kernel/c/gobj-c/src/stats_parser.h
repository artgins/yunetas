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

#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Prototypes
 **************************************************/
PUBLIC json_t *stats_parser(
    hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
);

PUBLIC json_t *build_stats( // Build stats from gobj's attributes with SFD_STATS flag.
    hgobj gobj,
    const char *stats,
    json_t *kw,
    hgobj src
);


#ifdef __cplusplus
}
#endif
