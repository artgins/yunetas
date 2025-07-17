/****************************************************************************
 *          YUNO_CONTROLCENTER.H
 *          Controlcenter yuno.
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_YUNO_CONTROLCENTER_NAME "YControlcenter"
#define ROLE_CONTROLCENTER "controlcenter"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void register_yuno_controlcenter(void);

#ifdef __cplusplus
}
#endif
