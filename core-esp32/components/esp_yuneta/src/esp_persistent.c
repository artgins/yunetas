/****************************************************************************
 *          loop.c
 *          GObj loop
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>

#ifdef ESP_PLATFORM
#include "nvs_flash.h"
#include "nvs.h"
#endif
#include <kwid.h>
#include "esp_persistent.h"

extern void jsonp_free(void *ptr);

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************************
 *
 ***************************************************************************/
int dbesp_startup_persistent_attrs(void)
{
#ifdef ESP_PLATFORM
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "nvs_flash_init() FAILED, flash erase",
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );

        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if(err != ESP_OK) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "nvs_flash_init() FAILED",
            "error",        "%s", esp_err_to_name(err),
            NULL
        );
    }
    ESP_ERROR_CHECK(err);
#endif
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
void dbesp_end_persistent_attrs(void)
{
#ifdef ESP_PLATFORM
    nvs_flash_deinit();
#endif
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *load_json(
    hgobj gobj
)
{
    char *svalue = NULL;
    json_t *jn = NULL;

#ifdef ESP_PLATFORM
    esp_err_t err;
    nvs_handle_t nvs_handle;
    err = nvs_open(gobj_gclass_name(gobj), NVS_READWRITE, &nvs_handle); // Don't use NVS_READONLY
    if(err != ESP_OK) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "nvs_open() FAILED",
            "namespace",    "%s", gobj_gclass_name(gobj),
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );
        return NULL;
    }

    size_t required_size;
    err = nvs_get_str(nvs_handle, gobj_name(gobj), NULL, &required_size);
    switch (err) {
        case ESP_OK:
            break;

        case ESP_ERR_NVS_NOT_FOUND:
            return NULL;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "nvs_get_str() FAILED",
                "namespace",    "%s", gobj_gclass_name(gobj),
                "key",          "%s", gobj_name(gobj),
                "esp_error",    "%s", esp_err_to_name(err),
                NULL
            );
            return NULL;
    }

    svalue = malloc(required_size);
    err = nvs_get_str(nvs_handle, gobj_name(gobj), svalue, &required_size);
    if(err != ESP_OK) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "nvs_get_str() 2 FAILED",
            "namespace",    "%s", gobj_gclass_name(gobj),
            "key",          "%s", gobj_name(gobj),
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );
        free(svalue);
        return NULL;
    }

    jn = anystring2json(gobj, svalue, strlen(svalue), TRUE);

    // Close
    nvs_close(nvs_handle);
#endif

    EXEC_AND_RESET(free, svalue)

    return jn;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_json(
    hgobj gobj,
    json_t *jn // owned
)
{
    char *s = json_dumps(jn, JSON_COMPACT|JSON_REAL_PRECISION(12));

#ifdef ESP_PLATFORM
    esp_err_t err;
    nvs_handle_t nvs_handle;
    err = nvs_open(gobj_gclass_name(gobj), NVS_READWRITE, &nvs_handle);
    if(err != ESP_OK) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "nvs_open() FAILED",
            "namespace",    "%s", gobj_gclass_name(gobj),
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );
        return -1;
    }

    err = nvs_set_str(nvs_handle, gobj_name(gobj), s);
    if(err != ESP_OK) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "nvs_set_str() FAILED",
            "namespace",    "%s", gobj_gclass_name(gobj),
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );
    }

    err = nvs_commit(nvs_handle);
    if(err != ESP_OK) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "nvs_commit() FAILED",
            "namespace",    "%s", gobj_gclass_name(gobj),
            "esp_error",    "%s", esp_err_to_name(err),
            NULL
        );
    }

    // Close
    nvs_close(nvs_handle);
#endif

    JSON_DECREF(jn)
    jsonp_free(s);
    return 0;
}

/***************************************************************************

 ***************************************************************************/
PUBLIC int dbesp_load_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);
    if(jn_file) {
        json_t *attrs = kw_clone_by_keys(
            gobj,
            jn_file,    // owned
            keys,       // owned
            FALSE
        );

        gobj_write_attrs(gobj, attrs, SDF_PERSIST, 0);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int dbesp_save_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_attrs = gobj_read_attrs(gobj, SDF_PERSIST, 0);
    json_t *attrs = kw_clone_by_keys(
        gobj,
        jn_attrs,   // owned
        keys,       // owned
        FALSE
    );

    json_t *jn_file = load_json(gobj);
    if(jn_file) {
        json_object_update_missing(attrs, jn_file);
        JSON_DECREF(jn_file)
    }

    save_json(
        gobj,
        attrs  // owned
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int dbesp_remove_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);

    json_t *attrs = kw_clone_by_not_keys(
        gobj,
        jn_file,    // owned
        keys,       // owned
        FALSE
    );

    save_json(
        gobj,
        attrs  // owned
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC json_t *dbesp_list_persistent_attrs(
    hgobj gobj,
    json_t *keys  // owned
) {
    json_t *jn_file = load_json(gobj);

    json_t *attrs = kw_clone_by_keys(
        gobj,
        jn_file,    // owned
        keys,       // owned
        FALSE
    );
    return attrs;
}
