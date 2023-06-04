/****************************************************************************
 *          environment.c
 *
 *          Environment
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#ifdef __linux__
  #include <unistd.h>
#endif
#ifdef ESP_PLATFORM
  #include <esp_mac.h>
  #include <esp_log.h>
#endif
#include "environment.h"

/***************************************************************
 *              Data
 ***************************************************************/
static char uuid[32] = {0};
static char hostname[64 + 1] = {0};

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *node_uuid(void)
{
    if(!uuid[0]) {
        uint8_t mac_addr[6] = {0};
#ifdef ESP_PLATFORM
        esp_efuse_mac_get_default(mac_addr);
#endif
        snprintf(uuid, sizeof(uuid), "ESP32-%02X-%02X-%02X-%02X-%02X-%02X",
            mac_addr[0],
            mac_addr[1],
            mac_addr[2],
            mac_addr[3],
            mac_addr[4],
            mac_addr[5]
        );
        gobj_log_info(0, 0,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "Mac address",
            "efuse_mac",    "%s", uuid,
            "uuid",         "%s", uuid,
            NULL
        );
    }
    return uuid;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC const char *get_hostname(void)
{
    if(!*hostname) {
#ifdef __linux__
        gethostname(hostname, sizeof(hostname)-1);
#endif
        if(!*hostname) {
            snprintf(hostname, sizeof(hostname), "esp32");
        }
    }
    return hostname;
}
