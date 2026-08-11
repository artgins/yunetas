/****************************************************************************
 *          TREEDB_SYSTEM_SCHEMA.H
 *
 *          Declaration of the treedb meta-schema literal.
 *          The string is single-source-of-truth for:
 *              - the cols of every treedb topic (consumed via
 *                _treedb_create_topic_cols_desc() in tr_treedb.c).
 *              - the __system__ treedb materialized by c_treedb.c
 *                in root-linux.
 *
 *          Mutable on purpose: helper_quote2doublequote() rewrites
 *          single quotes in place. The operation is idempotent, so it
 *          is safe to call from any consumer.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  Name of the treedb that stores schemas as data (the `id` of the literal
 *  below, and the name of the C_NODE service that C_TREEDB builds for it).
 *  A write to its topics is a schema change, so tr_treedb guards it.
 */
#define TREEDB_SYSTEM_SCHEMA_NAME   "treedb_system_schema"

extern char treedb_system_schema[];

#ifdef __cplusplus
}
#endif
