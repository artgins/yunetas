/****************************************************************************
 *          C_TREEDB.H
 *          Treedb GClass.
 *
 *          Management of treedb's
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <ginsfsm.h>
#include "yuneta_environment.h"
#include "c_yuno.h"
#include "c_node.h"
#include "msglog_yuneta.h"


#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_TREEDB_NAME "Treedb"
#define GCLASS_TREEDB gclass_treedb()

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_treedb(void);

#ifdef __cplusplus
}
#endif
