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
  #if CONFIG_ETH_USE_SPI_ETHERNET
    #include <driver/spi_master.h>
    #if CONFIG_ESTERILIZ_USE_ENC28J60
      #include "esp_eth_enc28j60.h"
    #endif //CONFIG_ESTERILIZ_USE_ENC28J60
  #endif // CONFIG_ETH_USE_SPI_ETHERNET
#endif // ESP_PLATFORM

#include "c_esp_yuno.h"
#include "c_esp_ethernet.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/
#ifdef ESP_PLATFORM
#endif

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type--------name----------------flag------------default---------description---------- */
SDATA (DTP_BOOLEAN, "periodic",         SDF_RD,         "0",            "True for periodic timeouts"),
SDATA (DTP_INTEGER, "msec",             SDF_RD,         "0",            "Timeout in miliseconds"),
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    BOOL periodic;
#ifdef ESP_PLATFORM
#endif
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
#ifdef ESP_PLATFORM
#endif

    SET_PRIV(periodic,          gobj_read_bool_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(periodic,    gobj_read_bool_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
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
#endif




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  Resend to the parent
 ***************************************************************************/
// PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
// {
//     gobj_send_event(gobj_parent(gobj), event, json_incref(kw), gobj);
//
//     JSON_DECREF(kw)
//     return 0;
// }




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
GOBJ_DEFINE_GCLASS(GC_ETHERNET);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_ETHERNET_IDLE);

/*------------------------*
 *      Events
 *------------------------*/
// Systems events, defined in gobj.c
//GOBJ_DEFINE_EVENT(EV_ETHERNET_TIMEOUT);
//GOBJ_DEFINE_EVENT(EV_ETHERNET_PERIODIC_TIMEOUT);

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
    ev_action_t st_idle[] = {
        {0,0,0}
    };
    states_t states[] = {
        {ST_ETHERNET_IDLE,      st_idle},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    gclass = gclass_create(
        gclass_name,
        0,  // event_types
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
    return create_gclass(GC_ETHERNET);
}


#ifdef PEPE
/***********************************************************************
 *          ethernet.c
 *
 *          Managing Olimex ethernet
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#define TAG "Ethernet ==>"

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_eth.h>
#include <esp_mac.h>
#include <driver/gpio.h>
#include <sdkconfig.h>
#if CONFIG_ETH_USE_SPI_ETHERNET
#include <driver/spi_master.h>
#if CONFIG_ESTERILIZ_USE_ENC28J60
#include "esp_eth_enc28j60.h"
#endif //CONFIG_ESTERILIZ_USE_ENC28J60
#endif // CONFIG_ETH_USE_SPI_ETHERNET

#include "c_esp_ethernet.h"

/***************************************************************
 *              Constants
 ***************************************************************/

/***************************************************************
 *              Data
 ***************************************************************/

/***************************************************************
 *              Prototypes
 ***************************************************************/

static esp_netif_ip_info_t ip;
static bool started = false;

/* acquiring for ip, could blocked here */
// TODO for what? xEventGroupWaitBits(eth_event_group, GOTIP_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

//static EventGroupHandle_t eth_event_group;
//static const int GOTIP_BIT = BIT0;

static esp_eth_handle_t eth_handle = NULL;
static esp_netif_t *eth_netif = NULL;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG,"ESP32 %s -> %d", event_base, (int)event_id);

//    ETHERNET_EVENT_START,        /*!< Ethernet driver start */
//    ETHERNET_EVENT_STOP,         /*!< Ethernet driver stop */
//    ETHERNET_EVENT_CONNECTED,    /*!< Ethernet got a valid link */
//    ETHERNET_EVENT_DISCONNECTED, /*!< Ethernet lost a valid link */

//    IP_EVENT_STA_GOT_IP,               /*!< station got IP from connected AP */
//    IP_EVENT_STA_LOST_IP,              /*!< station lost IP and the IP is reset to 0 */
//    IP_EVENT_AP_STAIPASSIGNED,         /*!< soft-AP assign an IP to a connected station */
//    IP_EVENT_GOT_IP6,                  /*!< station or ap or ethernet interface v6IP addr is preferred */
//    IP_EVENT_ETH_GOT_IP,               /*!< ethernet got IP from connected AP */
//    IP_EVENT_ETH_LOST_IP,              /*!< ethernet lost IP and the IP is reset to 0 */
//    IP_EVENT_PPP_GOT_IP,               /*!< PPP interface got IP */
//    IP_EVENT_PPP_LOST_IP,              /*!< PPP interface lost IP */

    if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_START) {
        started = true;
    } else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_STOP) {
        //xEventGroupClearBits(eth_event_group, GOTIP_BIT);
        started = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        memcpy(&ip, &event->ip_info, sizeof(ip));
        //xEventGroupSetBits(eth_event_group, GOTIP_BIT);

        uint8_t mac_addr[6];
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Hw addr: " MACSTR, MAC2STR(mac_addr));
        esp_netif_get_ip_info(eth_netif, &ip);
        ESP_LOGI(TAG,"Ethip: " IPSTR, IP2STR(&ip.ip));
        ESP_LOGI(TAG,"Ethmask: " IPSTR, IP2STR(&ip.netmask));
        ESP_LOGI(TAG,"Ethgw: " IPSTR, IP2STR(&ip.gw));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_LOST_IP) {
        //xEventGroupClearBits(eth_event_group, GOTIP_BIT); // TODO ??? for what?
    }
}

void register_ethernet(void)
{
    //eth_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&cfg);

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_ESTERILIZ_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_ESTERILIZ_ETH_PHY_RST_GPIO;
#if CONFIG_ESTERILIZ_USE_INTERNAL_ETHERNET
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_mdc_gpio_num = CONFIG_ESTERILIZ_ETH_MDC_GPIO;
    esp32_emac_config.smi_mdio_gpio_num = CONFIG_ESTERILIZ_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
#if CONFIG_ESTERILIZ_ETH_PHY_IP101
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_ESTERILIZ_ETH_PHY_RTL8201
    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_ESTERILIZ_ETH_PHY_LAN87XX
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
#elif CONFIG_ESTERILIZ_ETH_PHY_DP83848
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#elif CONFIG_ESTERILIZ_ETH_PHY_KSZ80XX
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz80xx(&phy_config);
#endif
#elif CONFIG_ETH_USE_SPI_ETHERNET
    gpio_install_isr_service(0);
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_ESTERILIZ_ETH_SPI_MISO_GPIO,
        .mosi_io_num = CONFIG_ESTERILIZ_ETH_SPI_MOSI_GPIO,
        .sclk_io_num = CONFIG_ESTERILIZ_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_ESTERILIZ_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_ESTERILIZ_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .spics_io_num = CONFIG_ESTERILIZ_ETH_SPI_CS_GPIO,
        .queue_size = 20
    };
#if CONFIG_ESTERILIZ_USE_KSZ8851SNL
    eth_ksz8851snl_config_t ksz8851snl_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(CONFIG_ESTERILIZ_ETH_SPI_HOST, &spi_devcfg);
    ksz8851snl_config.int_gpio_num = CONFIG_ESTERILIZ_ETH_SPI_INT_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz8851snl(&phy_config);
#elif CONFIG_ESTERILIZ_USE_DM9051
    eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(CONFIG_ESTERILIZ_ETH_SPI_HOST, &spi_devcfg);
    dm9051_config.int_gpio_num = CONFIG_ESTERILIZ_ETH_SPI_INT_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_dm9051(&phy_config);
#elif CONFIG_ESTERILIZ_USE_W5500
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_ESTERILIZ_ETH_SPI_HOST, &spi_devcfg);
    w5500_config.int_gpio_num = CONFIG_ESTERILIZ_ETH_SPI_INT_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);
#elif CONFIG_ESTERILIZ_USE_ENC28J60
    spi_devcfg.cs_ena_posttrans = enc28j60_cal_spi_cs_hold_time(CONFIG_ESTERILIZ_ETH_SPI_CLOCK_MHZ);
    eth_enc28j60_config_t enc28j60_config = ETH_ENC28J60_DEFAULT_CONFIG(CONFIG_ESTERILIZ_ETH_SPI_HOST, &spi_devcfg);
    enc28j60_config.int_gpio_num = CONFIG_ESTERILIZ_ETH_SPI_INT_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_enc28j60(&enc28j60_config, &mac_config);
    phy_config.autonego_timeout_ms = 0; // ENC28J60 doesn't support auto-negotiation
    phy_config.reset_gpio_num = -1; // ENC28J60 doesn't have a pin to reset internal PHY
    esp_eth_phy_t *phy = esp_eth_phy_new_enc28j60(&phy_config);
#endif
#endif // CONFIG_ETH_USE_SPI_ETHERNET
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
#if !CONFIG_ESTERILIZ_USE_INTERNAL_ETHERNET
    /* The SPI Ethernet module might doesn't have a burned factory MAC address, we cat to set it manually.
       02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
    */
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, (uint8_t[]) {
        0x02, 0x00, 0x00, 0x12, 0x34, 0x56
    }));
#endif
#if CONFIG_ESTERILIZ_USE_ENC28J60 && CONFIG_ESTERILIZ_ENC28J60_DUPLEX_FULL
    eth_duplex_t duplex = ETH_DUPLEX_FULL;
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_DUPLEX_MODE, &duplex));
#endif

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    uint8_t mac_addr[6];
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
    ESP_LOGI(TAG, "Hw addr: " MACSTR, MAC2STR(mac_addr));
    esp_netif_get_ip_info(eth_netif, &ip);
    ESP_LOGI(TAG,"Ethip: " IPSTR, IP2STR(&ip.ip));
    ESP_LOGI(TAG,"Ethmask: " IPSTR, IP2STR(&ip.netmask));
    ESP_LOGI(TAG,"Ethgw: " IPSTR, IP2STR(&ip.gw));
}

#endif
