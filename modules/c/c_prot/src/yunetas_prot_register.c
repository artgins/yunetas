/****************************************************************************
 *              YUNETA_IOT_REGISTER.C
 *              Yuneta
 *
 *              Copyright (c) 2022 Niyamaka.
 *              All Rights Reserved.
 ****************************************************************************/
#include "yuneta_iot.h"
#include "yuneta_iot_register.h"

/***************************************************************************
 *  Data
 ***************************************************************************/

/***************************************************************************
 *  Register internal yuno gclasses and services
 ***************************************************************************/
PUBLIC int yuneta_register_c_iot(void)
{
    static BOOL initialized = FALSE;
    if(initialized) {
        return -1;
    }

    /*
     *  Services
     */

    /*
     *  Gadgets
     */

    /*
     *  Protocols
     */
    gobj_register_gclass(GCLASS_PROT_MODBUS_MASTER);
    gobj_register_gclass(GCLASS_PROT_CANOPEN);
    gobj_register_gclass(GCLASS_GPS_SIM7600);
    gobj_register_gclass(GCLASS_MQTT);

    /*
     *  Mixin uv-gobj
     */
    gobj_register_gclass(GCLASS_CANBUS0);
    initialized = TRUE;

    return 0;
}

