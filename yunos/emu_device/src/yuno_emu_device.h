/****************************************************************************
 *          YUNO_EMU_DEVICE.H
 *          Emu_device yuno.
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#ifndef _YUNO_EMU_DEVICE_H
#define _YUNO_EMU_DEVICE_H 1

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_YUNO_EMU_DEVICE_NAME "YEmu_device"
#define ROLE_EMU_DEVICE "emu_device"

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC void register_yuno_emu_device(void);

#ifdef __cplusplus
}
#endif


#endif
