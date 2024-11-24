#include <esp_log.h>
#include <esp_wifi.h>
#include <mdns.h>
#include <string.h>

#include "filesystem.h"
#include "http_server.h"
#include "wlan.h"

/***************************
***** CONSTANTS ************
***************************/

#define TASK_CORE     1
#define TASK_PRIO     1
#define STACK_SIZE 4096

#define SCAN_MAX_AP             20
#define RECONNECT_TIMEOUT       30

#define WLAN_INT_MODE_REQ           1
#define WLAN_INT_AP_CONNECTED       2
#define WLAN_INT_AP_DISCONNECTED    3
#define WLAN_INT_STA_STARTED        4
#define WLAN_INT_CONNECTED          5
#define WLAN_INT_DISCONNECTED       6
#define WLAN_INT_REQ_RECONNECT      7

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
static void wlan_connect(void);
static void wlan_task(void *param);
static void wlan_timer_cb(TimerHandle_t timer);

/***************************
***** LOCAL VARIABLES ******
***************************/

static TaskHandle_t         handle;
static msg_type_t           msg_type;
static msg_type_t           msg_type_int;
static msg_handle_t         msg_handle;
static SemaphoreHandle_t    ap_mutex;
static uint16_t             ap_count;
static wlan_ap_t            ap_list[SCAN_MAX_AP];

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

    ap_mutex = xSemaphoreCreateMutex();

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

    ESP_ERROR_CHECK(esp_netif_set_hostname(intf_ap, CONFIG_WLAN_HOSTNAME));
    ESP_ERROR_CHECK(esp_netif_set_hostname(intf_sta, CONFIG_WLAN_HOSTNAME));

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(CONFIG_WLAN_HOSTNAME));
    ESP_ERROR_CHECK(mdns_service_add(CONFIG_MDNS_INSTANCE_NAME, CONFIG_MDNS_SERVICE_TYPE, CONFIG_MDNS_PROTOCOL, CONFIG_MDNS_PORT, NULL, 0));

    if (xTaskCreatePinnedToCore(&wlan_task, "wlan-task", STACK_SIZE, NULL, TASK_PRIO, &handle, TASK_CORE) != pdPASS) {
        LOGE("could not create task");
    }
}

msg_type_t wlan_msg_type(void) {
    assert(msg_type);
    return msg_type;
}

void wlan_toggle_mode(void) {
    assert(msg_type_int);
    msg_send_value(msg_type_int, WLAN_INT_MODE_REQ);
}

bool wlan_get_scan_result(uint8_t *cnt, wlan_ap_t **ap) {
    bool ret = false;
    assert(cnt);
    assert(ap);
    *cnt = 0;
    *ap = NULL;
    if (xSemaphoreTake(ap_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *cnt = ap_count;
        *ap = ap_list;
        ret = true;
    }
    return ret;
}

void wlan_free_scan_result(void) {
    xSemaphoreGive(ap_mutex);
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/

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
                msg_send_value(msg_type, WLAN_AP_STARTED);
                break;
            case WIFI_EVENT_AP_STOP:
                msg_send_value(msg_type, WLAN_AP_STOPPED);
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                msg_send_value(msg_type, WLAN_AP_CONNECTED);
                msg_send_value(msg_type_int, WLAN_INT_AP_CONNECTED);
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                msg_send_value(msg_type, WLAN_AP_DISCONNECTED);
                msg_send_value(msg_type_int, WLAN_INT_AP_DISCONNECTED);
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
    ESP_ERROR_CHECK(esp_wifi_stop());
}

static void wlan_start_ap(void) {
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_WLAN_SSID,
            .channel = 6,
            .password = CONFIG_WLAN_KEY,
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
    if (xSemaphoreTake(ap_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        wifi_ap_record_t records[SCAN_MAX_AP];
        ap_count = SCAN_MAX_AP;
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, records));
        LOGI("found %d APs", ap_count);
        for (int i = 0; i < ap_count; ++i) {
            LOGI("FOUND AP: SSID %s CH %d RSSI %d WPS %d AUTH %d, CC %s", records[i].ssid, records[i].primary, records[i].rssi, records[i].wps, records[i].authmode, records[i].country.cc);
            memcpy(ap_list[i].ssid, records[i].ssid, sizeof(ap_list[i].ssid));
            ap_list[i].rssi = records[i].rssi;
        }
        xSemaphoreGive(ap_mutex);
    } else {
        ESP_ERROR_CHECK(esp_wifi_clear_ap_list());
    }
    msg_send_value(msg_type, WLAN_SCAN_STOPPED);
}

static void wlan_connect(void) {
    uint8_t cnt;
    wlan_ap_t *ap;
    bool connect = false;
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = true
            },
        },
    };

    if (wlan_get_scan_result(&cnt, &ap)) {
        fs_wifi_cfg_t *cfg = fs_get_wifi_cfg();
        for (int i = 0; i < cnt ; ++i) {
            for (int j = 0; j < FS_NUMBER_OF_WIFI_NETWORKS; ++j) {
                if (!memcmp(ap[i].ssid, cfg->network[j].ssid, 32)) {
                    memcpy(&wifi_config.sta.ssid, cfg->network[j].ssid, sizeof(wifi_config.sta.ssid));
                    memcpy(&wifi_config.sta.password, cfg->network[j].key, sizeof(wifi_config.sta.password));
                    connect = true;
                    break;
                }
            }
        }
        fs_free_wifi_cfg(false);
        wlan_free_scan_result();
    }

    if (connect) {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

static void wlan_task(void *param) {
    TimerHandle_t timer = xTimerCreate("wifi-reconnect", pdMS_TO_TICKS(RECONNECT_TIMEOUT * 1000), false, NULL, wlan_timer_cb);
    wifi_mode_t mode;
    wlan_start_sta();
    for (;;) {
        msg_t msg = msg_receive(msg_handle);
        if (msg.type == msg_type_int) {
            ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
            switch (msg.value) {
                case WLAN_INT_MODE_REQ:
                    if (mode == WIFI_MODE_STA) {
                        xTimerStop(timer, 0);
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
                    msg_send_value(msg_type_int, WLAN_INT_REQ_RECONNECT);
                    break;
                case WLAN_INT_AP_CONNECTED:
                    http_start(CON_AP);
                    break;
                case WLAN_INT_AP_DISCONNECTED:
                    http_stop();
                    break;
                case WLAN_INT_CONNECTED:
                    xTimerStop(timer, 0);
                    http_start(CON_STA);
                    break;
                case WLAN_INT_DISCONNECTED:
                    http_stop();
                    if (xTimerIsTimerActive(timer) == pdFALSE) {
                        msg_send_value(msg_type_int, WLAN_INT_REQ_RECONNECT);
                    }
                    break;
                case WLAN_INT_REQ_RECONNECT:
                    if (mode == WIFI_MODE_STA) {
                        xTimerStart(timer, 0);
                        wlan_scan();
                        wlan_connect();
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

static void wlan_timer_cb(TimerHandle_t timer) {
    msg_send_value(msg_type_int, WLAN_INT_REQ_RECONNECT);
}
