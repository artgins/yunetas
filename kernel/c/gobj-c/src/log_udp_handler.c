/***********************************************************************
 *          log_udp_handler.c
 *
 *          Handler to seng log by udp
 *
 *          Copyright (c) 2014,2023 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#ifdef __linux__
    #include <fcntl.h>
    #include <syslog.h>
    #if __has_include(<linux/if.h>)
        #include <linux/if.h>
    #endif
#endif

#ifdef ESP_PLATFORM
#include <esp_event.h>
#include <esp_log.h>
#endif
#include "helpers.h"
#include "log_udp_handler.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
/*
 *  Syslog priority definitions
 */
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */
/*
 *  Extra mine priority definitions
 */
#define LOG_AUDIT       8       // written without header
#define LOG_MONITOR     9

/*
 *  IPv4 maximum reassembly buffer size is 576, IPv6 has it at 1500.
 *  512 is a safe value to use across the internet.
 */
#define DEFAULT_UDP_FRAME_SIZE  1500
#define DEFAULT_BUFFER_SIZE     (256*1024L)

#ifndef IFNAMSIZ
#define	IFNAMSIZ	16
#endif

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    DL_ITEM_FIELDS
    size_t udp_frame_size;
    char *buffer;   // dynamic mem of buffer_size bytes
    size_t buffer_size;
    output_format_t output_format;
    int _s;
    uint32_t counter;
    struct sockaddr_in si_other;
    char schema[32], host[64], port[32];
    char bindip[32];
    char if_name[IFNAMSIZ+1];
    BOOL exit_on_fail;
    BOOL disabled;
#ifdef ESP_PLATFORM
    esp_event_loop_handle_t tx_ev_loop_h;   // event loop with task to tx messages through task's callback
#endif
} udp_client_t;

/***************************************************************************
 *              Data
 ***************************************************************************/
PRIVATE char __initialized__ = 0;
PRIVATE char __process_name__[32] = {0};
PRIVATE char __hostname__[32] = {0};
PRIVATE int __pid__;
PRIVATE uint32_t sequence = 0;
PRIVATE dl_list_t dl_clients;
PRIVATE const char *months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int _udpc_socket(udp_client_t *uc);
PRIVATE int _udpc_write(udp_client_t *uc, char *bf, size_t len);
#ifdef ESP_PLATFORM
PRIVATE void udp_tx_ev_loop_callback(
    void *event_handler_arg,
    esp_event_base_t base,
    int32_t id,
    void *event_data
);
#endif


/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int udpc_start_up(const char *process_name, const char *hostname, int pid)
{
    if(__initialized__) {
        return -1;
    }
    if(process_name) {
        snprintf(__process_name__, sizeof(__process_name__), "%s", process_name);
    }
    if(hostname) {
        snprintf(__hostname__, sizeof(__hostname__), "%s", hostname);
    }
    __pid__ = pid;

    dl_init(&dl_clients, 0);
    __initialized__ = TRUE;
    return 0;
}

/***************************************************************************
 *  Close all
 ***************************************************************************/
PUBLIC void udpc_end(void)
{
    udp_client_t *uc;

    while((uc=dl_first(&dl_clients))) {
        udpc_close(uc);
    }
}

/***************************************************************************
 *  Return NULL on error
 ***************************************************************************/
PUBLIC udpc_t udpc_open(
    const char *url,
    const char *bindip,
    const char *if_name, // interface name
    size_t bfsize,
    size_t udp_frame_size,
    output_format_t output_format,
    BOOL exit_on_fail)
{
    udp_client_t *uc;

    /*-------------------------------------*
     *          Check parameters
     *-------------------------------------*/
    if(!url || !*url) {
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "Url empty");
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "Url empty");
        #endif
        return 0;
    }
    if(udp_frame_size == 0) {
        udp_frame_size = DEFAULT_UDP_FRAME_SIZE;
    }
    if(bfsize ==0) {
        bfsize = DEFAULT_BUFFER_SIZE;
    }
    if(output_format >= OUTPUT_LAST_ENTRY) {
        output_format = OUTPUT_FORMAT_YUNETA;
    }

    /*-------------------------------------*
     *          Alloc memory
     *-------------------------------------*/
    uc = malloc(sizeof(udp_client_t));
    if(!uc) {
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "No memory");
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "No memory");
        #endif
        return 0;
    }
    memset(uc, 0, sizeof(udp_client_t));
    if(!empty_string(bindip)) {
        snprintf(uc->bindip, sizeof(uc->bindip), "%s", bindip);
    }
    if(!empty_string(if_name)) {
        snprintf(uc->if_name, sizeof(uc->if_name), "%s", if_name);
    }

    uc->buffer_size = bfsize;
    uc->udp_frame_size = udp_frame_size;
    uc->output_format = output_format;
    uc->exit_on_fail = exit_on_fail;

    uc->buffer = malloc(uc->buffer_size + 1);    // +1 for the terminator null
    if(!uc->buffer) {
        free(uc);
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "No memory");
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "No memory");
        #endif
        return 0;
    }
    memset(uc->buffer, 0, uc->buffer_size + 1);

    /*-------------------------------------*
     *          Decode url
     *-------------------------------------*/
    int ret = parse_url(
        0,
        url,
        uc->schema, sizeof(uc->schema),
        uc->host, sizeof(uc->host),
        uc->port, sizeof(uc->port),
        0, 0,
        0, 0,
        FALSE
    );
    if(ret < 0) {
        free(uc->buffer);
        free(uc);
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "Bad url: %s", url);
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "Bad url: %s", url);
        #endif
        return 0;
    }

    /*-------------------------------------*
     *  Tx event loop for esp32
     *-------------------------------------*/
#ifdef ESP_PLATFORM
    esp_event_loop_args_t loop_handle_args = {
        .queue_size = 64,
        .task_name = url, // task will be created
        .task_priority = tskIDLE_PRIORITY,
        .task_stack_size = 4*1024,
        .task_core_id = tskNO_AFFINITY
    };
    ESP_ERROR_CHECK(esp_event_loop_create(&loop_handle_args, &uc->tx_ev_loop_h));
    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        uc->tx_ev_loop_h,           // event loop handle
        ESP_EVENT_ANY_BASE,         // event base
        ESP_EVENT_ANY_ID,           // event id
        udp_tx_ev_loop_callback,        // event handler
        uc,                         // event_handler_arg
        NULL                        // event handler instance, useful to unregister callback
    ));
#endif

    /*-------------------------------------*
     *  Add to list and create socket
     *-------------------------------------*/
    dl_add(&dl_clients, uc);
    if(_udpc_socket(uc)<0) {
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "_udpc_socket() FAILED");
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "_udpc_socket() FAILED");
        #endif
        // Retry open in next send
    }

    return uc;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC void udpc_close(udpc_t udpc)
{
    udp_client_t *uc = udpc;

    if(!udpc) {
        return;
    }
#ifdef ESP_PLATFORM
    if(uc->tx_ev_loop_h) {
        esp_event_loop_delete(uc->tx_ev_loop_h);
        uc->tx_ev_loop_h = 0;
    }
#endif
    if(uc->_s > 0) {
        close(uc->_s);
        uc->_s = 0;
    }
    dl_delete(&dl_clients, uc, 0);

    free(uc->buffer);
    free(uc);
}

/***************************************************************************
 *  max len 64K bytes
 ***************************************************************************/
PUBLIC int udpc_write(udpc_t udpc, int priority, const char* bf_, size_t len)
{
    udp_client_t *uc = udpc;
    const unsigned char* bf = (unsigned char*)bf_;

    if(!udpc || !bf) {
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "udpc or bf NULL");
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "udpc or bf NULL");
        #endif
        return -1;
    }

    size_t buffer_size = uc->buffer_size;
    size_t udp_frame_size = uc->udp_frame_size;

    if(len > buffer_size) {
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "buffer too long: %d > %d", (int)len, (int)buffer_size);
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "buffer too long: %d > %d", (int)len, (int)buffer_size);
        #endif
        return -1;
    }
    if(priority < 0 || priority > LOG_MONITOR) {
        priority = LOG_DEBUG;
    }

    /*
     *  Open or re-open the socket
     */
    if(uc->_s<=0) {
        if(!uc->disabled) {
            if(_udpc_socket(uc) < 0) {
                // Error already logged
                #ifdef ESP_PLATFORM
                    ESP_LOGE("YUNETA", "_udpc_socket(2) FAILED");
                #endif
                #ifdef __linux__
                    syslog(LOG_ERR, "YUNETA: " "_udpc_socket(2) FAILED");
                #endif
            }
        }
        return -1;
    }

    /*
     *  Format
     */
    switch(uc->output_format) {
    case OUTPUT_FORMAT_RAW:
        memcpy(uc->buffer, bf, len);
        *(uc->buffer+len) = 0; // add terminator null
        len++;
        break;

    case OUTPUT_FORMAT_SYSLOG:
        if(priority <= LOG_DEBUG) {
            #ifdef __linux__
            syslog(priority, "%s", bf);
            #endif
        }
        return 0;

    case OUTPUT_FORMAT_LUMBERJACK:
        {
            /*
             *  Example lumberjack syslog message:
             *
             *      'Mar 24 12:01:34 localhost sshd[12590]: @cee:{
             *          "msg": "Accepted publickey for algernon from 127.0.0.1 port 55519 ssh2",
             *          "pid": "12590", "facility": "auth", "priority": "info",
             *          "program": "sshd", "uid": "0", "gid": "0",
             *          "host": "hadhodrond", "timestamp": "2012-03-24T12:01:34.236987887+0100"
             *      }'
             *
             *  Needed format: "Mmm dd hh:mm:ss" (dd is %2d)
             */
            char timestamp[20];
            struct tm *tm;
            time_t t;

            if(priority > LOG_DEBUG) {
                /*
                 *  Don't send my priorities to lumberjack
                 */
                return 0;
            }

            time(&t);
            tm = localtime(&t);

            snprintf(timestamp, sizeof(timestamp), "%s %2d %02d:%02d:%02d",
                months[tm->tm_mon],
                tm->tm_mday,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec
            );

            snprintf(
                (char *)uc->buffer,
                uc->buffer_size,
                "<%d>%s %s %s[%d]: @cee:%.*s",
                    (1 << 3) | priority, // Always LOG_USER
                    timestamp,
                    __hostname__,
                    __process_name__,
                    __pid__,
                    (int)len,
                    bf
            );
            len = strlen((char *)uc->buffer);
            len++; // add final null
        }
        break;

    case OUTPUT_FORMAT_YUNETA:
    default:
        {
            uint32_t i, crc;

            unsigned char temp[12];
            snprintf(
                (char *)temp,
                sizeof(temp),
                "%c%08"PRIX32,
                '0' + priority,
                sequence
            );
            crc = 0;
            for(i=0; i<strlen((char *)temp); i++) {
                uint32_t x = (uint32_t)temp[i];
                crc += x;
            }
            for(i=0; i<len; i++) {
                uint32_t x = (uint32_t)bf[i];
                crc += x;
            }
            snprintf(
                (char *)uc->buffer,
                uc->buffer_size - 1,
                "%c%08"PRIX32"%s%08"PRIX32,
                '0' + priority,
                sequence,
                bf,
                crc
            );
            len = strlen((char *)uc->buffer);
            len++; // add final null
            sequence++;
        }
        break;
    }

    register char *p = uc->buffer;
    while(len > 0) {
        size_t sent = MIN(udp_frame_size, len);
        if(_udpc_write(uc, p, sent)<0) {
            close(uc->_s);
            uc->_s = 0;
            return -1;
        }
        len -= sent;
        p += sent;
    }

    return 0;
}

/*****************************************************************
 *
 *****************************************************************/
PUBLIC int udpc_fwrite(udpc_t udpc, int priority, const char *format, ...)
{
    udp_client_t *uc = udpc;
    va_list ap;
    char temp[1024];

    if(!udpc) {
        return -1;
    }
    va_start(ap, format);
    vsnprintf(
        temp,
        sizeof(temp),
        format,
        ap
    );
    va_end(ap);

    return udpc_write(uc, priority, temp, strlen(temp));
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _udpc_socket(udp_client_t *uc)
{
    uc->_s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(uc->_s<=0) {
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "socket() FAILED, errno %d, serrno %s", errno, strerror(errno));
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "socket() FAILED, errno %d, serrno %s", errno, strerror(errno));
        #endif
        return -1;
    }
    int opt = 1;
    setsockopt(uc->_s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

#ifdef __linux__
    int flags = fcntl(uc->_s, F_GETFL, 0);
    if(fcntl(uc->_s, F_SETFL, flags | O_NONBLOCK)<0) {
        syslog(LOG_ERR, "YUNETA: " "fcntl() FAILED, errno %d, serrno %s", errno, strerror(errno));
    }
#endif

    memset((char *) &uc->si_other, 0, sizeof(uc->si_other));
    uc->si_other.sin_family = AF_INET;
    uc->si_other.sin_port = htons((uint16_t)atoi(uc->port));

    //if (inet_aton(uc->host, &uc->si_other.sin_addr) == 0) {
    if(inet_pton(AF_INET, uc->host, &uc->si_other.sin_addr) < 0) {
        close(uc->_s);
        uc->_s = 0;
        #ifdef ESP_PLATFORM
            ESP_LOGE("YUNETA", "inet_pton() FAILED, errno %d, serrno %s", errno, strerror(errno));
        #endif
        #ifdef __linux__
            syslog(LOG_ERR, "YUNETA: " "inet_pton() FAILED, errno %d, serrno %s", errno, strerror(errno));
        #endif
        return -1;
    }

    if(!empty_string(uc->bindip)) {
        static struct sockaddr_in si_bind;
        memset((char *) &si_bind, 0, sizeof(si_bind));
        si_bind.sin_family = AF_INET;

        if (inet_pton(AF_INET, uc->bindip , &si_bind.sin_addr) < 0) {
            #ifdef ESP_PLATFORM
                ESP_LOGE("YUNETA", "inet_pton() FAILED, errno %d, serrno %s", errno, strerror(errno));
            #endif
            #ifdef __linux__
                syslog(LOG_ERR, "YUNETA: " "inet_pton() FAILED, errno %d, serrno %s", errno, strerror(errno));
            #endif
        } else if (bind(uc->_s, (const struct sockaddr *)&si_bind, sizeof(si_bind))<0) {
            #ifdef ESP_PLATFORM
                ESP_LOGE("YUNETA", "bind() FAILED, errno %d, serrno %s", errno, strerror(errno));
            #endif
            #ifdef __linux__
                syslog(LOG_ERR, "YUNETA: " "bind() FAILED, errno %d, serrno %s", errno, strerror(errno));
            #endif
        }
    }

    if(!empty_string(uc->if_name)) {
#ifdef ESP_PLATFORM
        ESP_LOGI("LOG_UDP", "Bind [sock=%d] to interface %s", uc->_s, uc->if_name);
#endif
        if (setsockopt(uc->_s, SOL_SOCKET, SO_BINDTODEVICE,  uc->if_name, IFNAMSIZ) != 0) {
#ifdef ESP_PLATFORM
            ESP_LOGE("LOG_UDP", "Bind [sock=%d] to interface %s fail", uc->_s, uc->if_name);
#endif
        }
    }

    char temp[80];
    snprintf(temp, sizeof(temp), "%s:%d", inet_ntoa(uc->si_other.sin_addr), ntohs(uc->si_other.sin_port));
    #ifdef ESP_PLATFORM
        ESP_LOGI("LOG_UDP_HANDLER", "socket(%d), ifr_name %s ==============================> %s",
            uc->_s, uc->if_name, temp
        );
    #endif

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _udpc_write(udp_client_t *uc, char *bf, size_t len)
{
#ifdef ESP_PLATFORM
    char *buf = malloc(len);
    if(buf) {
        memmove(buf, bf, len);
        esp_err_t err = esp_event_post_to(
            uc->tx_ev_loop_h,
            EV_SEND_MESSAGE,
            (int32_t)len,
            &buf,
            sizeof(char *),
            2
        );
        if (err != ESP_OK) {
            ESP_LOGE("YUNETA", "esp_event_post_to() FAILED");
            free(buf);
            return -1;
        }
    }
#endif

#ifdef __linux__
    if(sendto(uc->_s, bf, len, 0, (struct sockaddr *)&uc->si_other, sizeof(uc->si_other))<0) {
        close(uc->_s);
        uc->_s = 0;
        syslog(LOG_ERR, "YUNETA: " "sendto() FAILED, errno %d, serrno %s", errno, strerror(errno));
        return -1;
    }
#endif

    return 0;
}

/***************************************************************************
 *  Two types of data can be passed in to the event handler:
 *      - the handler specific data
 *      - the event-specific data.
 *
 *  - The handler specific data (handler_args) is a pointer to the original data,
 *  therefore, the user should ensure that the memory location it points to
 *  is still valid when the handler executes.
 *
 *  - The event-specific data (event_data) is a pointer to a deep copy of the original data,
 *  and is managed automatically.
***************************************************************************/
#ifdef ESP_PLATFORM
PRIVATE void udp_tx_ev_loop_callback(
    void *event_handler_arg,
    esp_event_base_t base,
    int32_t id,
    void *event_data
) {
    /*
     *  Manage tx_ev_loop_h
     */
    udp_client_t *uc = event_handler_arg;
    char *bf = *((char **) event_data);
    size_t len = (size_t)id;

    if(!uc->disabled) {
        if(sendto(uc->_s, bf, len, 0, (struct sockaddr *)&uc->si_other, sizeof(uc->si_other))<0) {
            ESP_LOGE("YUNETA", "sendto() FAILED, errno %d, serrno %s", errno, strerror(errno));
            if(errno != ENOMEM) {
                close(uc->_s);
                uc->_s = 0;
                uc->disabled = TRUE; // Wait to network on, yuno must re-open
            }
        }
    } else {
        ESP_LOGE("YUNETA", "%.*s", (int)len, bf);
    }

    free(bf);
}
#endif
