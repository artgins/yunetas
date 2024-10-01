/****************************************************************************
 *          C_TRANGER.H
 *          Resource GClass.
 *
 *          Resources with tranger
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 *
 *          Wrapper over tranger
 *
 ****************************************************************************/
#pragma once

#include <ginsfsm.h>
#include "yuneta_environment.h"
#include "c_timer.h"
#include "c_yuno.h"
#include "msglog_yuneta.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_TRANGER_NAME "Tranger"
#define GCLASS_TRANGER gclass_tranger()


/***************************************************************
 *              Structures
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_tranger(void);


#ifdef __cplusplus
}
#endif
