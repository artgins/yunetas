/****************************************************************************
 *          c_esp_ethernet.c
 *
 *          GClass Ethernet
 *          Low level esp-idf
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <time.h>
#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
    #include <freertos/FreeRTOS.h>
    #include <freertos/event_groups.h>
    #include <esp_netif.h>
    #include <esp_log.h>
    #include <esp_event.h>
    #include <esp_eth.h>
    #include <esp_mac.h>
    #include <driver/gpio.h>
    #include <sdkconfig.h>
    #include <esp_eth_mac.h>
    #if CONFIG_ETH_USE_SPI_ETHERNET
        #include <driver/spi_master.h>
        #if CONFIG_YUNETA_USE_ENC28J60
            #include <esp_eth_enc28j60.h>
        #endif //CONFIG_YUNETA_USE_ENC28J60
    #endif // CONFIG_ETH_USE_SPI_ETHERNET
#endif // ESP_PLATFORM

#include <helpers.h>
#include <kwid.h>
#include "c_timer.h"
#include "c_esp_yuno.h"
#include "c_esp_ethernet.h"

/***************************************************************
 *              Constants
 ***************************************************************/
#define OLIMEX_LED_PIN      33  /* TODO move to configuration, code repeated */

/***************************************************************
 *              Prototypes
 ***************************************************************/
#ifdef ESP_PLATFORM
PRIVATE void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
#endif
PRIVATE int start_ethernet(hgobj gobj);
PRIVATE int stop_ethernet(hgobj gobj);

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default---------description---------- */
//SDATA (DTP_BOOLEAN, "periodic",         SDF_RD,         "0",            "True for periodic timeouts"),
//SDATA (DTP_INTEGER, "msec",             SDF_RD,         "0",            "Timeout in miliseconds"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
#ifdef ESP_PLATFORM
    esp_netif_t *eth_netif;
    esp_eth_handle_t s_eth_handle ;
    esp_eth_mac_t *mac;
    esp_eth_phy_t *phy;
    esp_eth_netif_glue_handle_t s_eth_glue;
#endif
    hgobj gobj_periodic_timer;
    BOOL on_open_published;
    BOOL light_on;
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
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    priv->gobj_periodic_timer = gobj_create_pure_child("eth_periodic", C_TIMER, 0, gobj);

//    SET_PRIV(periodic,          gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);

//    IF_EQ_SET_PRIV(periodic,    gobj_read_bool_attr)
//    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->gobj_periodic_timer);
    start_ethernet(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->gobj_periodic_timer);
    gobj_stop(priv->gobj_periodic_timer);
    stop_ethernet(gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Local methods
                     ***************************/




/***************************************************************************
 *  Callback that will be executed when the timer period lapses.
 *  Posts the timer expiry event to the default event loop.
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

    if(event_base == ETH_EVENT) {
        /* we can get the ethernet driver handle from event data */
        esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

        switch(event_id) {
            case ETHERNET_EVENT_START:
                gobj_post_event(gobj, EV_ETHERNET_START, 0, gobj); // from esp_ethernet_start()
                processed = TRUE;
                break;
            case ETHERNET_EVENT_STOP:
                gobj_post_event(gobj, EV_ETHERNET_STOP, 0, gobj); // from esp_ethernet_stop()
                processed = TRUE;
                break;
            case ETHERNET_EVENT_CONNECTED:
                {
                    char temp[32];
                    uint8_t mac_addr[6] = {0};
                    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
                    snprintf(temp, sizeof(temp), "%02x:%02x:%02x:%02x:%02x:%02x",
                        mac_addr[0], mac_addr[1], mac_addr[2],
                        mac_addr[3], mac_addr[4], mac_addr[5]
                    );

                    gobj_log_info(gobj, 0,
                        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                        "msg",          "%s", "Ethernet Link Up",
                        "mac",          "%s", temp,
                        NULL
                    );

                    kw = json_pack("{s:s}",
                        "mac", temp
                    );
                    gobj_post_event(gobj, EV_ETHERNET_CONNECTED, kw, gobj);
                    processed = TRUE;
                }
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                /*
                 *  WARNING: sockets must be closed and recreated
                 */
                {
                    gobj_log_info(gobj, 0,
                        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                        "msg",          "%s", "Ethernet Link Down",
                        NULL
                    );

                    kw = json_object();
                    gobj_post_event(gobj, EV_ETHERNET_DISCONNECTED, kw, gobj);
                    processed = TRUE;
                }
                break;

            default:
                break;
        }

    } else if(event_base == IP_EVENT) {
        switch(event_id) {
            case IP_EVENT_ETH_GOT_IP:
                /*
                 *  WARNING
                 *  Upon receiving this event, the application needs to close all sockets
                 *  and recreate the application when the IPV4 changes to a valid one.
                 */
                {
                    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
                    const esp_netif_ip_info_t *ip_info = &event->ip_info;
                    char ip[32], mask[32], gateway[32];

                    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info->ip));
                    snprintf(mask, sizeof(mask), IPSTR, IP2STR(&ip_info->netmask));
                    snprintf(gateway, sizeof(gateway), IPSTR, IP2STR(&ip_info->gw));

                    gobj_log_info(gobj, 0,
                        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                        "msg",          "%s", "Ethernet Got IP Address",
                        "Ip",           "%s", ip,
                        "Mask",         "%s", mask,
                        "Gateway",      "%s", gateway,
                        "ip_changed",   "%d", (int)event->ip_changed,
                        NULL
                    );

                    kw = json_pack("{s:s, s:s, s:s, s:i}",
                        "ip", ip,
                        "mask", mask,
                        "gateway", gateway,
                        "ip_changed", (int)event->ip_changed
                    );
                    gobj_post_event(gobj, EV_ETHERNET_GOT_IP, kw, gobj);
                    processed = TRUE;
                }
                break;

            case IP_EVENT_ETH_LOST_IP:
                // It seems that may be ignored, only to debug
                gobj_post_event(gobj, EV_ETHERNET_LOST_IP, 0, gobj);
                processed = TRUE;
                break;

            case IP_EVENT_GOT_IP6:
            default:
                break;
        }
    }
    if(!processed) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "ESP ETHERNET event not processed",
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
#if CONFIG_YUNETA_USE_INTERNAL_ETHERNET
PRIVATE int eth_configure_internal(
    hgobj gobj,
    eth_mac_config_t *mac_config,
    eth_phy_config_t *phy_config
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_mdc_gpio_num = CONFIG_YUNETA_ETH_MDC_GPIO;
    esp32_emac_config.smi_mdio_gpio_num = CONFIG_YUNETA_ETH_MDIO_GPIO;
    priv->mac = esp_eth_mac_new_esp32(&esp32_emac_config, mac_config);

#if CONFIG_YUNETA_ETH_PHY_IP101
    priv->phy = esp_eth_phy_new_ip101(phy_config);
#elif CONFIG_YUNETA_ETH_PHY_RTL8201
    priv->phy = esp_eth_phy_new_rtl8201(phy_config);
#elif CONFIG_YUNETA_ETH_PHY_LAN87XX
    priv->phy = esp_eth_phy_new_lan87xx(phy_config);
#elif CONFIG_YUNETA_ETH_PHY_DP83848
    priv->phy = esp_eth_phy_new_dp83848(phy_config);
#elif CONFIG_YUNETA_ETH_PHY_KSZ80XX
    priv->phy = esp_eth_phy_new_ksz80xx(phy_config);
#endif

    return 0;
}
#endif /* CONFIG_YUNETA_USE_INTERNAL_ETHERNET */

/***************************************************************************
 *
 ***************************************************************************/
#if CONFIG_YUNETA_USE_SPI_ETHERNET
PRIVATE int eth_configure_spi(
    hgobj gobj,
    eth_mac_config_t *mac_config,
    eth_phy_config_t *phy_config
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gpio_install_isr_service(0);
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_YUNETA_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_YUNETA_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_YUNETA_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_YUNETA_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_YUNETA_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_YUNETA_ETH_SPI_CS_GPIO,
        .queue_size = 20
    };

#if CONFIG_YUNETA_USE_KSZ8851SNL
    eth_ksz8851snl_config_t ksz8851snl_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(CONFIG_YUNETA_ETH_SPI_HOST, &spi_devcfg);
    ksz8851snl_config.int_gpio_num = CONFIG_YUNETA_ETH_SPI_INT_GPIO;
    priv->mac = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config, &mac_config);
    priv->phy = esp_eth_phy_new_ksz8851snl(&phy_config);
#elif CONFIG_YUNETA_USE_DM9051
    eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(CONFIG_YUNETA_ETH_SPI_HOST, &spi_devcfg);
    dm9051_config.int_gpio_num = CONFIG_YUNETA_ETH_SPI_INT_GPIO;
    priv->mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
    priv->phy = esp_eth_phy_new_dm9051(&phy_config);
#elif CONFIG_YUNETA_USE_W5500
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_YUNETA_ETH_SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = CONFIG_YUNETA_ETH_SPI_INT_GPIO;
    priv->mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    priv->phy = esp_eth_phy_new_w5500(&phy_config);
#elif CONFIG_YUNETA_USE_ENC28J60
    spi_devcfg.cs_ena_posttrans = enc28j60_cal_spi_cs_hold_time(CONFIG_YUNETA_ETH_SPI_CLOCK_MHZ);
    eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(CONFIG_YUNETA_ETH_SPI_HOST, &spi_devcfg);
    enc28j60_config.int_gpio_num = CONFIG_YUNETA_ETH_SPI_INT_GPIO;
    priv->mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);
    phy_config.autonego_timeout_ms = 0; // ENC28J60 doesn't support auto-negotiation
    phy_config.reset_gpio_num = -1; // ENC28J60 doesn't have a pin to reset internal PHY
    priv->phy = esp_eth_phy_new_enc28j60(&phy_config);
#endif

    return 0;
}
#endif /* CONFIG_YUNETA_USE_SPI_ETHERNET */

/***************************************************************************
 *
 ***************************************************************************/
#if CONFIG_YUNETA_USE_OPENETH
PRIVATE int eth_configure_openeth(
    hgobj gobj,
    eth_mac_config_t *mac_config,
    eth_phy_config_t *phy_config
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    phy_config->autonego_timeout_ms = 100;
    priv->mac = esp_eth_mac_new_openeth(mac_config);
    priv->phy = esp_eth_phy_new_dp83848(phy_config);

    return 0;
}
#endif

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int start_ethernet(hgobj gobj)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    priv->eth_netif = esp_netif_new(&cfg);
    if(!priv->eth_netif) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "esp_netif_new() FAILED",
            NULL
        );
        return -1;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    //mac_config.rx_task_stack_size = CONFIG_EXAMPLE_ETHERNET_EMAC_TASK_STACK_SIZE;

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_YUNETA_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_YUNETA_ETH_PHY_RST_GPIO;


#if CONFIG_YUNETA_USE_INTERNAL_ETHERNET
    eth_configure_internal(gobj, &mac_config, &phy_config);

#elif CONFIG_YUNETA_USE_SPI_ETHERNET
    eth_configure_spi(gobj, &mac_config, &phy_config);

#elif CONFIG_YUNETA_USE_OPENETH
    eth_configure_openeth(gobj, &mac_config, &phy_config);

#else
    ESP_LOGE(TAG, "no Ethernet device selected to init");
    abort();
#endif

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(priv->mac, priv->phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &priv->s_eth_handle));

#if !CONFIG_YUNETA_USE_INTERNAL_ETHERNET
    /* The SPI Ethernet module might doesn't have a burned factory MAC address, we cat to set it manually.
       02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
    */
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, (uint8_t[]) {
        0x02, 0x00, 0x00, 0x12, 0x34, 0x56
    }));
#endif

    priv->s_eth_glue = esp_eth_new_netif_glue(priv->s_eth_handle);
    esp_netif_attach(priv->eth_netif, priv->s_eth_glue);

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &event_handler, gobj));
    // WARNING: duplicate in c_esp_wifi.c :
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, gobj));

    ESP_ERROR_CHECK(esp_eth_start(priv->s_eth_handle));

#endif /* ESP_PLATFORM */

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int stop_ethernet(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    // WARNING: duplicate in c_esp_wifi.c :
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler));

    if(priv->s_eth_handle) {
        esp_eth_stop(priv->s_eth_handle);
        priv->s_eth_handle = NULL;
    }
    if(priv->s_eth_glue) {
        esp_eth_del_netif_glue(priv->s_eth_glue);
        priv->s_eth_glue = NULL;
    }
    if(priv->s_eth_handle) {
        esp_eth_driver_uninstall(priv->s_eth_handle);
        priv->s_eth_handle = NULL;
    }

    if(priv->phy) {
        priv->phy->del(priv->phy);
        priv->phy = NULL;
    }

    if(priv->mac) {
        priv->mac->del(priv->mac);
        priv->mac = NULL;
    }

    if(priv->eth_netif) {
        esp_netif_destroy(priv->eth_netif);
        priv->eth_netif = NULL;
    }
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_ethernet_start(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "ethernet_start",
        NULL
    );

#ifdef ESP_PLATFORM
#endif

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_ethernet_stop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "ethernet_stop",
        NULL
    );

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  WARNING: sockets must be closed and recreated
 ***************************************************************************/
PRIVATE int ac_ethernet_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "ethernet_disconnected",
        NULL
    );

    if(priv->on_open_published) {
        priv->on_open_published =  FALSE;
        if(!gobj_is_shutdowning()) {
            gobj_publish_event(gobj, EV_ETHERNET_ON_CLOSE, json_incref(kw));
        }
    }
#endif
    clear_timeout(priv->gobj_periodic_timer);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_periodic(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
#ifdef ESP_PLATFORM
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_current_state(gobj) == ST_ETHERNET_IP_ASSIGNED) {
        gpio_set_level(OLIMEX_LED_PIN, 1);
        priv->light_on = 1;
    }
#endif

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_ethernet_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "ethernet connected",
        NULL
    );

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_ethernet_got_ip(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_info(gobj, 0,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "got ip",
        NULL
    );

    set_timeout_periodic(priv->gobj_periodic_timer, 500);

    priv->on_open_published = TRUE;
    gobj_publish_event(gobj, EV_ETHERNET_ON_OPEN, json_incref(kw)); // Wait to play default_service until get time

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
GOBJ_DEFINE_GCLASS(C_ETHERNET);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_ETHERNET_WAIT_START);
GOBJ_DEFINE_STATE(ST_ETHERNET_WAIT_CONNECTED);
GOBJ_DEFINE_STATE(ST_ETHERNET_WAIT_IP);
GOBJ_DEFINE_STATE(ST_ETHERNET_IP_ASSIGNED);

/*------------------------*
 *      Events
 *------------------------*/
// Systems events, defined in gobj.c
GOBJ_DEFINE_EVENT(EV_ETHERNET_START);
GOBJ_DEFINE_EVENT(EV_ETHERNET_STOP);
GOBJ_DEFINE_EVENT(EV_ETHERNET_CONNECTED);
GOBJ_DEFINE_EVENT(EV_ETHERNET_DISCONNECTED);
GOBJ_DEFINE_EVENT(EV_ETHERNET_GOT_IP);
GOBJ_DEFINE_EVENT(EV_ETHERNET_LOST_IP);
GOBJ_DEFINE_EVENT(EV_ETHERNET_ON_OPEN);
GOBJ_DEFINE_EVENT(EV_ETHERNET_ON_CLOSE);

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
    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_ethernet_wait_start[] = {
        {EV_ETHERNET_START,             ac_ethernet_start,              ST_ETHERNET_WAIT_CONNECTED},
        {0,0,0}
    };
    ev_action_t st_ethernet_wait_connected[] = { // From connect_station()
        {EV_ETHERNET_DISCONNECTED,      ac_ethernet_disconnected,       0},
        {EV_ETHERNET_CONNECTED,         ac_ethernet_connected,          ST_ETHERNET_WAIT_IP},
        {EV_ETHERNET_STOP,              ac_ethernet_stop,               ST_ETHERNET_WAIT_START},
        {0,0,0}
    };
    ev_action_t st_ethernet_wait_ip[] = {
        {EV_ETHERNET_GOT_IP,            ac_ethernet_got_ip,             ST_ETHERNET_IP_ASSIGNED},
        {EV_ETHERNET_DISCONNECTED,      ac_ethernet_disconnected,       ST_ETHERNET_WAIT_CONNECTED},
        {EV_ETHERNET_STOP,              ac_ethernet_stop,               ST_ETHERNET_WAIT_START},
        {0,0,0}
    };
    ev_action_t st_ethernet_ip_assigned[] = {
        {EV_TIMEOUT_PERIODIC,           ac_timeout_periodic,            0},
        {EV_ETHERNET_DISCONNECTED,      ac_ethernet_disconnected,       ST_ETHERNET_WAIT_CONNECTED},
        {EV_ETHERNET_STOP,              ac_ethernet_stop,               ST_ETHERNET_WAIT_START},
        {0,0,0}
    };

    states_t states[] = {
        {ST_ETHERNET_WAIT_START,            st_ethernet_wait_start},
        {ST_ETHERNET_WAIT_CONNECTED,        st_ethernet_wait_connected},
        {ST_ETHERNET_WAIT_IP,               st_ethernet_wait_ip},
        {ST_ETHERNET_IP_ASSIGNED,           st_ethernet_ip_assigned},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ETHERNET_START,             0},
        {EV_ETHERNET_STOP,              0},
        {EV_ETHERNET_CONNECTED,         0},
        {EV_ETHERNET_DISCONNECTED,      0},
        {EV_ETHERNET_GOT_IP,            0},
        {EV_ETHERNET_LOST_IP,           0},
        {EV_ETHERNET_ON_OPEN,           EVF_OUTPUT_EVENT},
        {EV_ETHERNET_ON_CLOSE,          EVF_OUTPUT_EVENT},
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
        0   // gclass_flag
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
PUBLIC int register_c_esp_ethernet(void)
{
    return create_gclass(C_ETHERNET);
}
