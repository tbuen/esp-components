#include <esp_log.h>
#include <esp_wifi.h>
#include <mdns.h>

#include "message.h"
#include "http_server.h"
#include "wlan.h"

/***************************
***** CONSTANTS ************
***************************/

#define TASK_CORE     1
#define TASK_PRIO     1
#define STACK_SIZE 4096

#define SCAN_MAX_AP  20

#define WLAN_INT_MODE_REQ       1
#define WLAN_INT_AP_STARTED     2
#define WLAN_INT_STA_STARTED    3
#define WLAN_INT_CONNECTED      4
#define WLAN_INT_DISCONNECTED   5

/***************************
***** MACROS ***************
***************************/

#define TAG "wlan"

#define LOGE(...) ESP_LOGE(TAG, __VA_ARGS__)
#define LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#define LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#define LOGD(...) ESP_LOGD(TAG, __VA_ARGS__)

/***************************
***** TYPES ****************
***************************/

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static void wlan_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wlan_start_sta(void);
static void wlan_stop_sta(void);
static void wlan_start_ap(void);
static void wlan_stop_ap(void);
static void wlan_scan(void);
static void wlan_task(void *param);

/***************************
***** LOCAL VARIABLES ******
***************************/

static TaskHandle_t handle;
static msg_type_t   msg_type;
static msg_type_t   msg_type_int;
static msg_handle_t msg_handle;

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void wlan_init(void) {
    assert(!handle);
    assert(!msg_type);
    assert(!msg_type_int);

    msg_type = msg_register();
    msg_type_int = msg_register();

    msg_handle = msg_listen(msg_type_int);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *intf_ap = esp_netif_create_default_wifi_ap();
    esp_netif_t *intf_sta = esp_netif_create_default_wifi_sta();

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(intf_ap, &ip_info);
    LOGI("AP IP address: " IPSTR, IP2STR(&ip_info.ip));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_country_code("DEI", true));
    char country[10] = {0};
    ESP_ERROR_CHECK(esp_wifi_get_country_code(country));
    LOGI("Country Code: %s", country);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wlan_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wlan_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_netif_set_hostname(intf_ap, "esp32-audio"));
    ESP_ERROR_CHECK(esp_netif_set_hostname(intf_sta, "esp32-audio"));

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set("esp32-audio"));
    ESP_ERROR_CHECK(mdns_service_add("audio", "_audio-jsonrpc-ws", "_tcp", 80, NULL, 0));

    if (xTaskCreatePinnedToCore(&wlan_task, "wlan-task", STACK_SIZE, NULL, TASK_PRIO, &handle, TASK_CORE) != pdPASS) {
        LOGE("could not create task");
    }
}

msg_type_t  wlan_msg_type(void) {
    assert(msg_type);
    return msg_type;
}

void wlan_toggle_mode(void) {
    assert(msg_type_int);
    msg_send_value(msg_type_int, WLAN_INT_MODE_REQ);
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/

/*void wlan_set_mode(wlan_mode_t m) {
    if (m == WLAN_MODE_STA) {
        request_t request = { WLAN_REQ_STA, NULL };
        xQueueSendToBack(request_queue, &request, 0);
    } else if (m == WLAN_MODE_AP) {
        request_t request = { WLAN_REQ_AP, NULL };
        xQueueSendToBack(request_queue, &request, 0);
    }
}

void wlan_connect(const uint8_t *ssid, const uint8_t *password) {
    wifi_msg_t *wifi_msg = calloc(1, sizeof(wifi_msg_t));
    memcpy(wifi_msg->ssid, ssid, sizeof(wifi_msg->ssid));
    memcpy(wifi_msg->password, password, sizeof(wifi_msg->password));
    request_t request = { WLAN_REQ_CONNECT, wifi_msg };
    xQueueSendToBack(request_queue, &request, 0);
}*/

static void wlan_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                msg_send_value(msg_type_int, WLAN_INT_STA_STARTED);
                break;
            case WIFI_EVENT_STA_STOP:
                break;
            case WIFI_EVENT_SCAN_DONE:
                break;
            case WIFI_EVENT_STA_CONNECTED:
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                LOGI("STA disconnected, reason: %d", ((wifi_event_sta_disconnected_t*)event_data)->reason);
                msg_send_value(msg_type_int, WLAN_INT_DISCONNECTED);
                msg_send_value(msg_type, WLAN_DISCONNECTED);
                break;
            case WIFI_EVENT_AP_START:
                msg_send_value(msg_type_int, WLAN_INT_AP_STARTED);
                msg_send_value(msg_type, WLAN_AP_STARTED);
                break;
            case WIFI_EVENT_AP_STOP:
                msg_send_value(msg_type, WLAN_AP_STOPPED);
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                msg_send_value(msg_type, WLAN_AP_CONNECTED);
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                msg_send_value(msg_type, WLAN_AP_DISCONNECTED);
                break;
            default:
                break;
        }
    }
    if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
            {
                ip_event_got_ip_t *event = (ip_event_got_ip_t*)event_data;
                LOGI("GOT IP: " IPSTR, IP2STR(&event->ip_info.ip));
                msg_send_value(msg_type_int, WLAN_INT_CONNECTED);
                msg_send_value(msg_type, WLAN_CONNECTED);
                break;
            }
            case IP_EVENT_STA_LOST_IP:
                LOGI("LOST IP");
                break;
            default:
                break;
        }
    }
}

static void wlan_start_sta(void) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wlan_stop_sta(void) {
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    //wlan_scan();
    ESP_ERROR_CHECK(esp_wifi_stop());
}

static void wlan_start_ap(void) {
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "esp32-audio",
            .channel = 6,
            .password = "audio-esp",
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wlan_stop_ap(void) {
    ESP_ERROR_CHECK(esp_wifi_deauth_sta(0));
    ESP_ERROR_CHECK(esp_wifi_stop());
}

static void wlan_scan(void) {
    wifi_scan_config_t config = {
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    msg_send_value(msg_type, WLAN_SCAN_STARTED);
    ESP_ERROR_CHECK(esp_wifi_scan_start(&config, true));
    uint16_t number = SCAN_MAX_AP;
    wifi_ap_record_t records[SCAN_MAX_AP];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, records));
    LOGI("found %d APs", number);
    for (int i = 0; i < number; ++i) {
        LOGI("FOUND AP: SSID %s CH %d RSSI %d WPS %d AUTH %d, CC %s", records[i].ssid, records[i].primary, records[i].rssi, records[i].wps, records[i].authmode, records[i].country.cc);
    }
    msg_send_value(msg_type, WLAN_SCAN_STOPPED);
}

/*static void wlan_con(const uint8_t *ssid, const uint8_t *password) {
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                //.capable = true,
                //.required = false
                .required = true
            },
        },
    };
    memcpy(&wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(&wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}*/

static void wlan_task(void *param) {
    wifi_mode_t mode;
    wlan_start_sta();
    for (;;) {
        msg_t msg = msg_receive(msg_handle);
        if (msg.type == msg_type_int) {
            switch (msg.value) {
                case WLAN_INT_MODE_REQ:
                    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
                    if (mode == WIFI_MODE_STA) {
                        http_stop();
                        wlan_stop_sta();
                        wlan_start_ap();
                    } else if (mode == WIFI_MODE_AP) {
                        http_stop();
                        wlan_stop_ap();
                        wlan_start_sta();
                    }
                    break;
                case WLAN_INT_STA_STARTED:
                    wlan_scan();
                    break;
                case WLAN_INT_AP_STARTED:
                    http_start(CON_AP);
                    break;
                case WLAN_INT_CONNECTED:
                    http_start(CON_STA);
                    break;
                case WLAN_INT_DISCONNECTED:
                    http_stop();
                    break;
                default:
                    break;
            }
        }
        /*request_t request;
        if (xQueueReceive(request_queue, &request, portMAX_DELAY) == pdTRUE) {
            switch (request.req) {
                case WLAN_REQ_STA:
                    if (mode == WIFI_MODE_AP) {
                        wlan_ap(false);
                    }
                    wlan_sta(true);
                    mode = WIFI_MODE_STA;
                    break;
                case WLAN_REQ_AP:
                    if (mode == WIFI_MODE_STA) {
                        wlan_sta(false);
                    }
                    wlan_ap(true);
                    mode = WIFI_MODE_AP;
                    break;
#if 0
                case WLAN_REQ_CONNECT:
                    if (   (mode == WIFI_MODE_STA)
                        && request.data) {
                        wifi_msg_t *wifi_msg = (wifi_msg_t*)request.data;
                        wlan_con(wifi_msg->ssid, wifi_msg->password);
                        free(request.data);
                    }
                    break;
#endif
                default:
                    break;
            }
        }*/
    }
}
