/****************************************************************************
 *          C_NODE.H
 *          Resource GClass.
 *
 *          Resources with treedb
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <ginsfsm.h>
#include "yuneta_environment.h"
#include "c_timer.h"
#include "c_yuno.h"
#include "c_tranger.h"
#include "msglog_yuneta.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_NODE_NAME "Node"
#define GCLASS_NODE gclass_node()


/***************************************************************
 *              Structures
 ***************************************************************/

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
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_node(void);


#ifdef __cplusplus
}
#endif
