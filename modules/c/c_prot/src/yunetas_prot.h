/****************************************************************************
 *              YUNETA_IOT.H
 *              Includes
 *              Copyright (c) 2022 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/*
 *  core
 */
#include <yuneta.h>
#include "msglog_iot.h"
#include "yuneta_iot_version.h"
#include "yuneta_iot_register.h"

/*
 *  Services
 */

/*
 *  Gadgets
 */

/*
 *  Protocols
 */
#include "c_prot_modbus_master.h"
#include "c_prot_canopen.h"
#include "c_gps_sim7600.h"
#include "c_mqtt.h"

/*
 *  Mixin uv-gobj
 */
#include "c_canbus0.h"


#ifdef __cplusplus
}
#endif
