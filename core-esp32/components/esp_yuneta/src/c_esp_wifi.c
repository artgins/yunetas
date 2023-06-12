/****************************************************************************
 *          c_esp_wifi.c
 *
 *          GClass Wifi
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM
  #include <esp_wifi.h>
  #include <esp_wpa2.h>
  #include <esp_event.h>
  #include <esp_log.h>
  #include <esp_netif.h>
  #include <esp_smartconfig.h>
#endif

#include <helpers.h>
#include <kwid.h>
#include "c_esp_yuno.h"
#include "c_esp_wifi.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define OLIMEX_LED_PIN      33

/***************************************************************
 *              Prototypes
 ***************************************************************/
#ifdef ESP_PLATFORM
PRIVATE void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
#endif
PRIVATE int connect_station(hgobj gobj);
PRIVATE int start_smartconfig(hgobj gobj);

/***************************************************************
 *              Data
 ***************************************************************/
//PRIVATE json_desc_t jn_wifi_desc[] = {
//// Name         Type        Default
//{"ssid",        "str",      ""},
//{"password",    "str",      ""},
//{"bssid",       "str",      ""},
//{"bssid_set",   "bool",     "false"},
//{0}   // HACK important, final null
//};

/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag--------------------default-----description---------- */
SDATA (DTP_STRING,  "mac_address",      SDF_RD|SDF_STATS,       "",         "Wifi mac address"),
SDATA (DTP_INTEGER, "rssi",             SDF_RD|SDF_STATS,       "",         "Wifi RSSI"),

//SDATA (DTP_BOOLEAN, "login_data_saved", SDF_PERSIST|SDF_STATS,  "false",    "Wifi data saved"),
//SDATA (DTP_STRING,  "ssid",             SDF_PERSIST|SDF_STATS,  "",         "Wifi ssid (33)"),
//SDATA (DTP_STRING,  "password",         SDF_PERSIST,            "",         "Wifi password (65)"),
//SDATA (DTP_STRING,  "bssid",            SDF_PERSIST,            "",         "Wifi bssid (6)"), // not used
//SDATA (DTP_BOOLEAN, "bssid_set",        SDF_PERSIST,            "false",    "Wifi bssid set"), // not used

SDATA (DTP_JSON,    "wifi_list",        SDF_PERSIST,            "[]",       "List of wifis (ssid/passw)"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
#ifdef ESP_PLATFORM
    esp_netif_t *sta_netif;
#endif
    BOOL on_open_published;
    int idx_wifi_list;
} PRIVATE_DATA;

PRIVATE hgclass gclass = 0;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->sta_netif = esp_netif_create_default_wifi_sta();
    assert(priv->sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, gobj));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, gobj));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, gobj));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
#endif
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

#ifdef ESP_PLATFORM
    ESP_ERROR_CHECK(esp_wifi_start()); // Will arise WIFI_EVENT_STA_START
#endif

    priv->idx_wifi_list = -1;

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
#ifdef ESP_PLATFORM
    esp_wifi_stop();    // Will arise WIFI_EVENT_STA_STOP
#endif
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
#ifdef ESP_PLATFORM
    esp_wifi_deinit();
#endif
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *  Callback that will be executed when the wifi period lapses.
 *  Posts the wifi expiry event to the default event loop.
 ***************************************************************************/
#ifdef ESP_PLATFORM
/***************************************************************************
 *
 ***************************************************************************/
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    hgobj gobj = arg;
    json_t *kw = 0;
    BOOL processed = FALSE;
    uint8_t ssid[32+1];
    char ip[20];
    memset(ssid, 0, sizeof(ssid));

    if(event_base == WIFI_EVENT) {
        wifi_event_sta_disconnected_t *wifi_event_sta_disconnected;
        wifi_event_sta_connected_t *wifi_event_sta_connected;

        switch(event_id) {
            case WIFI_EVENT_STA_START:
                gobj_post_event(gobj, EV_WIFI_STA_START, 0, gobj); // from esp_wifi_start()
                processed = TRUE;
                break;
            case WIFI_EVENT_STA_STOP:
                gobj_post_event(gobj, EV_WIFI_STA_STOP, 0, gobj); // from esp_wifi_stop()
                processed = TRUE;
                break;
            case WIFI_EVENT_SCAN_DONE:
                {
                    #define DEFAULT_SCAN_LIST_SIZE 10
                    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
                    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
                    uint16_t ap_count = 0;
                    memset(ap_info, 0, sizeof(ap_info));

                    esp_wifi_scan_get_ap_records(&number, ap_info);
                    esp_wifi_scan_get_ap_num(&ap_count);
                    json_t *kw_scan = json_object();
                    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
                        json_t *jn_ssid = json_object();
                        json_object_set_new(jn_ssid, "SSID", json_string((const char *)ap_info[i].ssid));
                        json_object_set_new(jn_ssid, "RSSI", json_integer(ap_info[i].rssi));
                        json_object_set_new(jn_ssid, "authmode", json_integer(ap_info[i].authmode));
                        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
                            json_object_set_new(
                                jn_ssid, "pairwise_cipher", json_integer(ap_info[i].pairwise_cipher)
                            );
                            json_object_set_new(
                                jn_ssid, "group_cipher", json_integer(ap_info[i].group_cipher)
                            );
                        }
                        json_object_set_new(jn_ssid, "channel", json_integer(ap_info[i].primary));
                        json_object_set_new(kw_scan, (const char *)ap_info[i].ssid, jn_ssid);
                    }
                    gobj_post_event(gobj, EV_WIFI_SCAN_DONE, kw_scan, gobj); // from esp_wifi_scan_start()
                }
                processed = TRUE;
                break;
            case WIFI_EVENT_STA_CONNECTED:
                wifi_event_sta_connected = event_data;
                memcpy(ssid, wifi_event_sta_connected->ssid, wifi_event_sta_connected->ssid_len);
                kw = json_pack("{s:s, s:i, s:i}",
                    "ssid", ssid,
                    "channel", (int)wifi_event_sta_connected->channel,
                    "authmode", (int)wifi_event_sta_connected->authmode
                );
                gobj_post_event(gobj, EV_WIFI_STA_CONNECTED, kw, gobj); // from esp_wifi_connect()
                processed = TRUE;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                /*
                 *  From esp_wifi_disconnect() or esp_wifi_stop()
                 *      - in this case, do not re-call esp_wifi_connect()
                 *  From esp_wifi_connect() when scan fails or the authentication times out
                 *      - in this case you can re-call esp_wifi_connect()
                 *
                 *  WARNING: sockets must be closed and recreated
                 */
                wifi_event_sta_disconnected = event_data;
                memcpy(ssid, wifi_event_sta_disconnected->ssid, wifi_event_sta_disconnected->ssid_len);
                kw = json_pack("{s:s, s:i, s:i}",
                    "ssid", ssid,
                    "reason", (int)wifi_event_sta_disconnected->reason,
                    "rssi", (int)wifi_event_sta_disconnected->rssi
                );
                gobj_post_event(gobj, EV_WIFI_STA_DISCONNECTED, kw, gobj);
                processed = TRUE;
                break;
            case WIFI_EVENT_STA_BEACON_TIMEOUT:
                // This happens when wifi is shutdown
                // Do nothing, next came WIFI_EVENT_STA_DISCONNECTED
                processed = TRUE;
                break;

            default:
                break;
        }

    } else if(event_base == IP_EVENT) {
        ip_event_got_ip_t *ip_event_got_ip;
        switch(event_id) {
            case IP_EVENT_STA_GOT_IP:
                /*
                 *  WARNING
                 *  Upon receiving this event, the application needs to close all sockets
                 *  and recreate the application when the IPV4 changes to a valid one.
                 */
                ip_event_got_ip = (ip_event_got_ip_t*)event_data;
                snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_event_got_ip->ip_info.ip));
                kw = json_pack("{s:s, s:i}",
                    "ip", ip,
                    "ip_changed", (int)ip_event_got_ip->ip_changed
                );
                gobj_post_event(gobj, EV_WIFI_GOT_IP, kw, gobj);
                processed = TRUE;
                break;

            case IP_EVENT_STA_LOST_IP:
                // It seems that may be ignored, only to debug
                gobj_post_event(gobj, EV_WIFI_LOST_IP, 0, gobj);
                processed = TRUE;
            default:
                break;
        }


    } else if(event_base == SC_EVENT) {
        switch(event_id) {
            case SC_EVENT_SCAN_DONE:
            case SC_EVENT_FOUND_CHANNEL:
                processed = TRUE;
                break;
            case SC_EVENT_GOT_SSID_PSWD:
                {
                    char bssid[32];     /**< MAC address of target AP. */
                    memset(bssid, 0, sizeof(bssid));
                    smartconfig_event_got_ssid_pswd_t *evt = event_data;
                    snprintf(bssid, sizeof(bssid), "%02X.%02X.%02X.%02X.%02X.%02X",
                        evt->bssid[0],
                        evt->bssid[1],
                        evt->bssid[2],
                        evt->bssid[3],
                        evt->bssid[4],
                        evt->bssid[5]
                    );
                    kw = json_pack("{s:s, s:s, s:b, s:s}",
                        "ssid", evt->ssid,
                        "password", evt->password,
                        "bssid_set", evt->bssid_set,
                        "bssid", bssid
                    );

                    gobj_post_event(gobj, EV_WIFI_SMARTCONFIG_DONE, kw, gobj);
                    processed = TRUE;
                }
                break;
            case SC_EVENT_SEND_ACK_DONE:
                processed = TRUE;
                gobj_post_event(gobj, EV_WIFI_SMARTCONFIG_ACK_DONE, kw, gobj);
                break;
            default:
                break;
        }
    }
    if(!processed) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "ESP WIFI event not processed",
            "event_base",   "%s", event_base,
            "event_id",     "%d", (int)event_id,
            NULL
        );
    }
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int start_smartconfig(hgobj gobj)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "DO start_smartconfig",
        NULL
    );

#ifdef ESP_PLATFORM
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
#endif

    json_t *jn_wifi_list = gobj_read_json_attr(gobj, "wifi_list");
    if(json_array_size(jn_wifi_list) == 0) {
        /*
         *  if the wifi list is empty then change to ST_WIFI_WAIT_SSID_CONF, wait forever
         */
        gobj_change_state(gobj, ST_WIFI_WAIT_SSID_CONF);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int connect_station(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "DO connect_station",
        NULL
    );

    json_t *jn_wifi_list = gobj_read_json_attr(gobj, "wifi_list");
    gobj_trace_json(gobj, jn_wifi_list, "connect_station------------------->wifi_list"); // TODO TEST

    int max_wifi_list = (int)json_array_size(jn_wifi_list);
    if(max_wifi_list > 0) {
        int x = ++priv->idx_wifi_list % max_wifi_list;
        priv->idx_wifi_list = x;
        json_t *jn_wifi = json_array_get(jn_wifi_list, priv->idx_wifi_list);

        gobj_trace_json(gobj, jn_wifi, "Using %d------------------->wifi_list", priv->idx_wifi_list); // TODO TEST
    }

    json_t *jn_wifi = json_array_get(jn_wifi_list, priv->idx_wifi_list);

    gobj_trace_json(gobj, jn_wifi, "Using %d------------------->wifi_list", priv->idx_wifi_list); // TODO TEST

#ifdef ESP_PLATFORM
    wifi_config_t wifi_config;
    const char *ssid = kw_get_str(gobj, jn_wifi, "ssid", "", KW_REQUIRED);
    const char *password = kw_get_str(gobj, jn_wifi, "password", "", KW_REQUIRED);
    //const char *bssid = kw_get_str(gobj, jn_wifi, "bssid", "", KW_REQUIRED);
    /*
     *  Generally, station_config.bssid_set needs to be 0;
     *  and it needs to be 1 only when users need to check the MAC address of the AP.
     */
    BOOL bssid_set = 0; //kw_get_bool(gobj, jn_wifi, "bssid_set", 0, KW_REQUIRED);

    bzero(&wifi_config, sizeof(wifi_config_t));
    memcpy(wifi_config.sta.ssid, ssid, MIN(strlen(ssid), sizeof(wifi_config.sta.ssid)));
    memcpy(wifi_config.sta.password, password, MIN(strlen(password),sizeof(wifi_config.sta.password)));
    wifi_config.sta.bssid_set = bssid_set;
    if (wifi_config.sta.bssid_set == true) {
        //memcpy(wifi_config.sta.bssid, bssid, MIN(strlen(bssid), sizeof(wifi_config.sta.bssid)));
    }

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if(err != 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "esp_wifi_set_config() FAILED",
            "error",        "%s", esp_err_to_name(err),
            NULL
        );
        return -1;
    }
    esp_wifi_connect();
#endif
    gobj_change_state(gobj, ST_WIFI_WAIT_STA_CONNECTED);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void get_rssi(hgobj gobj)
{
    /*
     *  Get RSSI (Received Signal Strength Indicator)
     */
#ifdef ESP_PLATFORM
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);

    gobj_log_info(gobj, 0,
        "msgset",   "%s", MSGSET_CONNECTION,
        "msg",      "%s", "RSSI",
        "rssi",     "%d", (int) ap_info.rssi,
        NULL
    );
    gobj_write_integer_attr(gobj, "rssi", (int)ap_info.rssi);
#endif
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_wifi_start(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  Get Mac-address
     */
#ifdef ESP_PLATFORM
    char mac_addr[32];
    esp_wifi_get_mac(ESP_IF_WIFI_STA, (uint8_t *)mac_addr);
    snprintf(mac_addr, sizeof(mac_addr), "%02X-%02X-%02X-%02X-%02X-%02X",
         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]
    );
    gobj_write_str_attr(gobj, "mac_address", mac_addr);

    esp_wifi_scan_start(NULL, false);
#endif

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_wifi_stop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "wifi_stop",
        NULL
    );

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  From esp_wifi_disconnect() or esp_wifi_stop()
 *      - in this case, do not re-call esp_wifi_connect()
 *  From esp_wifi_connect() when scan fails or the authentication times out
 *      - in this case you can re-call esp_wifi_connect()
 *
 *  Do connect_station() or start_smartconfig()
 *
 *  WARNING: sockets must be closed and recreated
 ***************************************************************************/
PRIVATE int ac_wifi_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    wifi_err_reason_t reason = (int)kw_get_int(gobj, kw, "reason", 0, 0);
    int rssi = (int)kw_get_int(gobj, kw, "rssi", 0, 0);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "wifi_disconnected",
        "ssid",         "%s", kw_get_str(gobj, kw, "ssid", "", 0),
        "reason",       "%d", reason,
        "rssi",         "%d", rssi,
        NULL
    );

    if(priv->on_open_published) {
        priv->on_open_published =  FALSE;
        if(!gobj_is_shutdowning()) {
            gobj_publish_event(gobj, EV_WIFI_ON_CLOSE, json_incref(kw));
        }
    }

    switch(reason) {
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_NOT_AUTHED:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: // password incorrect
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            gobj_log_info(gobj, 0,
                "msgset",       "%s", MSGSET_CONNECTION,
                "msg",          "%s", "wifi_disconnected by WRONG PASSWORD",
                "ssid",         "%s", kw_get_str(gobj, kw, "ssid", "", 0),
                "reason",       "%d", reason,
                "rssi",         "%d", rssi,
                NULL
            );
            break;

        case WIFI_REASON_NO_AP_FOUND: // wifi shutdown
        default:
            break;
    }
#endif

    if(gobj_is_running(gobj)) {
        connect_station(gobj);  // change to ST_WIFI_WAIT_STA_CONNECTED, wait forever
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_scan_done(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "scan done",
        NULL
    );
    gobj_trace_json(gobj, kw, "scan done");

    json_t *jn_wifi_list = gobj_read_json_attr(gobj, "wifi_list");
    if(json_array_size(jn_wifi_list) > 0) {
        connect_station(gobj);      // change to ST_WIFI_WAIT_STA_CONNECTED, wait forever
    }

    start_smartconfig(gobj);    // change to ST_WIFI_WAIT_SSID_CONF if empty wifi list, wait forever

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_smartconfig_done_connect(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "smartconfig done, connect",
        NULL
    );

    const char *id = kw_get_str(gobj, kw, "ssid", "", KW_REQUIRED);
    if(empty_string(id)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "ssid empty",
            NULL
        );
        JSON_DECREF(kw)
        return -1;
    }

    json_object_set_new(kw, "id", json_string(id));

    json_t *jn_wifi_list = gobj_read_json_attr(gobj, "wifi_list");
    json_t *jn_record = kwid_get(gobj, jn_wifi_list, id, 0, 0);
    if(!jn_record) {
        json_array_insert(jn_wifi_list, 0, kw);
    } else  {
        json_object_update(jn_record, kw);
    }
    gobj_save_persistent_attrs(gobj, json_string("wifi_list"));

    gobj_trace_json(gobj, jn_wifi_list, "ac_smartconfig_done_connect DONE"); // TODO TEST

    connect_station(gobj);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_smartconfig_done_save(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "smartconfig done, save",
        NULL
    );

    const char *id = kw_get_str(gobj, kw, "ssid", "", KW_REQUIRED);
    if(empty_string(id)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "ssid empty",
            NULL
        );
        JSON_DECREF(kw)
        return -1;
    }

    json_object_set_new(kw, "id", json_string(id));

    json_t *jn_wifi_list = gobj_read_json_attr(gobj, "wifi_list");
    json_t *jn_record = kwid_get(gobj, jn_wifi_list, id, 0, 0);
    if(!jn_record) {
        json_array_insert(jn_wifi_list, 0, kw);
    } else  {
        json_object_update(jn_record, kw);
    }
    gobj_save_persistent_attrs(gobj, json_string("wifi_list"));

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_smartconfig_ack_done(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "smartconfig ACK done",
        NULL
    );

#ifdef ESP_PLATFORM
    esp_smartconfig_stop();
#endif
    start_smartconfig(gobj);    // change to ST_WIFI_WAIT_SSID_CONF if empty wifi list, wait forever

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_wifi_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "wifi connected",
        NULL
    );

    get_rssi(gobj);

#ifdef ESP_PLATFORM
#endif

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_wifi_got_ip(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECTION,
        "msg",          "%s", "got ip",
        NULL
    );

// TODO get time
//    sntp_setoperatingmode(SNTP_OPMODE_POLL);
//    sntp_setservername(0, "pool.ntp.org");
//    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
//    sntp_init();
//    sntp_servermode_dhcp(1);
//
//    // wait for time to be set
//    int retry = 0;
//    const int retry_count = 10;
//    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
//        vTaskDelay(2000 / portTICK_PERIOD_MS);
//    }

    priv->on_open_published = TRUE;
    gobj_publish_event(gobj, EV_WIFI_ON_OPEN, json_incref(kw)); // Wait to play default_service until get time

    JSON_DECREF(kw)
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_WIFI);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_WIFI_WAIT_START);
GOBJ_DEFINE_STATE(ST_WIFI_WAIT_SCAN);
GOBJ_DEFINE_STATE(ST_WIFI_WAIT_SSID_CONF);
GOBJ_DEFINE_STATE(ST_WIFI_WAIT_STA_CONNECTED);
GOBJ_DEFINE_STATE(ST_WIFI_WAIT_IP);
GOBJ_DEFINE_STATE(ST_WIFI_IP_ASSIGNED);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_WIFI_STA_START);
GOBJ_DEFINE_EVENT(EV_WIFI_STA_STOP);
GOBJ_DEFINE_EVENT(EV_WIFI_STA_CONNECTED);
GOBJ_DEFINE_EVENT(EV_WIFI_STA_DISCONNECTED);
GOBJ_DEFINE_EVENT(EV_WIFI_SCAN_DONE);
GOBJ_DEFINE_EVENT(EV_WIFI_SMARTCONFIG_DONE);
GOBJ_DEFINE_EVENT(EV_WIFI_SMARTCONFIG_ACK_DONE);
GOBJ_DEFINE_EVENT(EV_WIFI_GOT_IP);
GOBJ_DEFINE_EVENT(EV_WIFI_LOST_IP);
GOBJ_DEFINE_EVENT(EV_WIFI_ON_OPEN);
GOBJ_DEFINE_EVENT(EV_WIFI_ON_CLOSE);

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    if(gclass) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_wifi_wait_start[] = {
        {EV_WIFI_STA_START,             ac_wifi_start,              ST_WIFI_WAIT_SCAN},
        {0,0,0}
    };
    ev_action_t st_wifi_wait_scan[] = {
        {EV_WIFI_SCAN_DONE,             ac_scan_done,               0},
        // Do connect_station() or start_smartconfig()

        {EV_WIFI_STA_STOP,              ac_wifi_stop,               ST_WIFI_WAIT_START},
        {0,0,0}
    };
    ev_action_t st_wifi_wait_ssid_conf[] = { // From start_smartconfig()
        {EV_WIFI_SMARTCONFIG_DONE,      ac_smartconfig_done_connect,0}, // Do connect_station()
        {EV_WIFI_STA_STOP,              ac_wifi_stop,               ST_WIFI_WAIT_START},
        {0,0,0}
    };
    ev_action_t st_wifi_wait_sta_connected[] = { // From connect_station()
        {EV_WIFI_STA_DISCONNECTED,      ac_wifi_disconnected,       0},
        {EV_WIFI_STA_CONNECTED,         ac_wifi_connected,          ST_WIFI_WAIT_IP},
        {EV_WIFI_STA_STOP,              ac_wifi_stop,               ST_WIFI_WAIT_START},
        {EV_WIFI_SMARTCONFIG_DONE,      ac_smartconfig_done_save,   0},
        {EV_WIFI_SMARTCONFIG_ACK_DONE,  ac_smartconfig_ack_done,    0},
        {0,0,0}
    };
    ev_action_t st_wifi_wait_ip[] = {
        {EV_WIFI_GOT_IP,                ac_wifi_got_ip,             ST_WIFI_IP_ASSIGNED},
        {EV_WIFI_STA_DISCONNECTED,      ac_wifi_disconnected,       0},
        {EV_WIFI_STA_STOP,              ac_wifi_stop,               ST_WIFI_WAIT_START},
        {EV_WIFI_SMARTCONFIG_DONE,      ac_smartconfig_done_save,   0},
        {EV_WIFI_SMARTCONFIG_ACK_DONE,  ac_smartconfig_ack_done,    0},
        {0,0,0}
    };
    ev_action_t st_wifi_ip_assigned[] = {
        {EV_WIFI_STA_DISCONNECTED,      ac_wifi_disconnected,       0},
        {EV_WIFI_STA_STOP,              ac_wifi_stop,               ST_WIFI_WAIT_START},
        {EV_WIFI_SMARTCONFIG_DONE,      ac_smartconfig_done_save,   0},
        {EV_WIFI_SMARTCONFIG_ACK_DONE,  ac_smartconfig_ack_done,    0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_WIFI_WAIT_START,            st_wifi_wait_start},
        {ST_WIFI_WAIT_SCAN,             st_wifi_wait_scan},
        {ST_WIFI_WAIT_SSID_CONF,        st_wifi_wait_ssid_conf},
        {ST_WIFI_WAIT_STA_CONNECTED,    st_wifi_wait_sta_connected},
        {ST_WIFI_WAIT_IP,               st_wifi_wait_ip},
        {ST_WIFI_IP_ASSIGNED,           st_wifi_ip_assigned},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_WIFI_STA_START,             0},
        {EV_WIFI_STA_STOP,              0},
        {EV_WIFI_STA_CONNECTED,         0},
        {EV_WIFI_STA_DISCONNECTED,      0},
        {EV_WIFI_SMARTCONFIG_DONE,      0},
        {EV_WIFI_GOT_IP,                0},
        {EV_WIFI_LOST_IP,               0},
        {EV_WIFI_ON_OPEN,               EVF_OUTPUT_EVENT},
        {EV_WIFI_ON_CLOSE,              EVF_OUTPUT_EVENT},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
        gcflag_singleton   // gcflag_t
    );
    if(!gclass) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_esp_wifi(void)
{
    return create_gclass(C_WIFI);
}
