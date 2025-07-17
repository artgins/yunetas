/****************************************************************************
 *          C_EMU_DEVICE.H
 *          Emu_device GClass.
 *
 *          Emulator of device gates
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#ifndef _C_EMU_DEVICE_H
#define _C_EMU_DEVICE_H 1

#include <yuneta.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Constants
 ***************************************************************/
#define GCLASS_EMU_DEVICE_NAME "Emu_device"
#define GCLASS_EMU_DEVICE gclass_emu_device()

/***************************************************************
 *              Prototypes
 ***************************************************************/
PUBLIC GCLASS *gclass_emu_device(void);

#ifdef __cplusplus
}
#endif

#endif
