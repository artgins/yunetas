/****************************************************************************
 *          gobj_environment.c
 *
 *          Environment
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#ifdef __linux__
    #include <unistd.h>
    #include <limits.h>
    #include <sys/stat.h>
    #include <sys/statvfs.h>
    #include <dirent.h>
    #include <uuid/uuid.h>
    #include <jansson.h>
#endif
#ifdef ESP_PLATFORM
    #include <esp_mac.h>
    #include <esp_log.h>
#endif
#include "gobj_environment.h"
#include "helpers.h"
#include "kwid.h"

/***************************************************************
 *              Data
 ***************************************************************/
static char uuid[64] = {0};
static char hostname[64 + 1] = {0};

/***************************************************************************
 *
 ***************************************************************************/
#ifdef __linux__
PRIVATE void save_uuid(const char *uuid_)
{
    snprintf(uuid, sizeof(uuid), "%s", uuid_);

    char *directory = "/yuneta/store/agent/uuid";
    json_t *jn_uuid = json_object();
    json_object_set_new(jn_uuid, "uuid", json_string(uuid_));

    save_json_to_file(
        NULL,
        directory,
        "uuid.json",
        02770,
        0660,
        0,
        TRUE,   //create
        FALSE,  //only_read
        jn_uuid // owned
    );
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
#ifdef __linux__
PRIVATE const char *read_node_uuid(void)
{
   json_t *jn_uuid = load_json_from_file(
       NULL,
        "/yuneta/store/agent/uuid",
        "uuid.json",
        0
    );

    if(jn_uuid) {
        const char *uuid_ = kw_get_str(0, jn_uuid, "uuid", "", KW_REQUIRED);
        snprintf(uuid, sizeof(uuid), "%s", uuid_);
        json_decref(jn_uuid);
    }
    return uuid;
}
#endif

/***************************************************************************
 *  En raspberry pi:
        Hardware        : BCM2835
        Revision        : a020d3
        Serial          : 00000000a6d12d83
 ***************************************************************************/
#ifdef __linux__
PRIVATE const char *_calculate_uuid_by_cpuinfo(void)
{
    const char *filename = "/proc/cpuinfo";
    static char new_uuid[64] = {0};
    char buffer[LINE_MAX];
    char hardware[16] = {0};
    char revision[16] = {0};
    char serial[32] = {0};

    new_uuid[0] = 0;

    BOOL found = FALSE;
    FILE *file = fopen(filename, "r");
    if(file) {
        while(fgets(buffer, sizeof(buffer), file)) {
            if(strncasecmp(buffer, "Hardware", strlen("Hardware")) ==0) {
                char *p = strchr(buffer, ':');
                p++;
                left_justify(p);
                snprintf(hardware, sizeof(hardware), "%s", p);
            }
            if(strncasecmp(buffer, "Revision", strlen("Revision")) ==0) {
                char *p = strchr(buffer, ':');
                p++;
                left_justify(p);
                snprintf(revision, sizeof(revision), "%s", p);
            }
            if(strncasecmp(buffer, "Serial", strlen("Serial")) ==0) {
                char *p = strchr(buffer, ':');
                p++;
                left_justify(p);
                snprintf(serial, sizeof(serial), "%s", p);
                found = TRUE;
            }
        }
        fclose(file);
    }
    if(found) {
        snprintf(new_uuid, sizeof(new_uuid), "%s-%s-%s", hardware, revision, serial);
    }
    return new_uuid;
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
#ifdef __linux__
PRIVATE const char *_calculate_uuid_by_uuid(void)
{
    static char new_uuid[64] = {0};
    uuid_t uuid_;

    uuid_generate(uuid_); // Generate a UUID
    uuid_unparse(uuid_, new_uuid); // Convert UUID to string

    return new_uuid;
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
#ifdef __linux__
PRIVATE const char *_calculate_new_uuid(void)
{
    const char *uuid_by_cpuinfo = _calculate_uuid_by_cpuinfo();
    if(!empty_string(uuid_by_cpuinfo)) {
        // Raspberry
        return uuid_by_cpuinfo;
    } else {
        return _calculate_uuid_by_uuid();
    }
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
#ifdef __linux__
PUBLIC const char *generate_node_uuid(void)
{
    const char *cur_uuid = read_node_uuid();
    if(!empty_string(cur_uuid)) {
        return cur_uuid;
    }

    const char *new_uuid = _calculate_new_uuid();
    save_uuid(new_uuid);

    return new_uuid;
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC const char *node_uuid(void)
{
    if(!uuid[0]) {
#ifdef ESP_PLATFORM
        uint8_t mac_addr[6] = {0};
        esp_efuse_mac_get_default(mac_addr);
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
#endif
#ifdef __linux__
        generate_node_uuid();
#endif
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
#ifdef ESP_PLATFORM
        if(!*hostname) {
            snprintf(hostname, sizeof(hostname), "%s", "esp32");
        }
#endif
    }
    return hostname;
}
