/****************************************************************************
 *            register_prot.c
 *
 *            Register several protocols
 *
 *            Copyright (c) 2022 Niyamaka.
 *            Copyright (c) 2025, ArtGins.
 *            All Rights Reserved.
 ****************************************************************************/
#include "c_prot_modbus_m.h"
#include "c_ota.h"
#include "register_prot.h"

/***************************************************************************
 *  Register protocols gclass
 ***************************************************************************/
PUBLIC int register_prot(void)
{
    static BOOL initialized = false;
    if(initialized) {
        return -1;
    }

    register_c_ota();
    register_c_prot_modbus_m();

    initialized = true;

    return 0;
}
