/****************************************************************************
 *          C_NODE.H
 *          Resource GClass.
 *
 *          Resources with treedb
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 *
 *  HACK All data conversion must be going through json.
 *  SData and others are peripherals, leaf nodes. Json is a through-node.
 *
 *  Here the edges are the conversion algorithms.
 *  In others are the communication links.
 *
 *  Is true "What is above, is below. What is below, is above"?
 *
 */

/***************************************************************
 *              FSM
 ***************************************************************/
/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DECLARE_GCLASS(C_NODE);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DECLARE_EVENT(EV_TREEDB_UPDATE_NODE);
GOBJ_DECLARE_EVENT(EV_TREEDB_NODE_CREATED);
GOBJ_DECLARE_EVENT(EV_TREEDB_NODE_UPDATED);
GOBJ_DECLARE_EVENT(EV_TREEDB_NODE_DELETED);

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC int register_c_node(void);


#ifdef __cplusplus
}
#endif
