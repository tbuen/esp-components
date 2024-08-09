#pragma once

#include "message.h"

/********************
***** CONSTANTS *****
********************/

#define WLAN_CONNECTED       1
#define WLAN_DISCONNECTED    2
#define WLAN_AP_STARTED      3
#define WLAN_AP_STOPPED      4
#define WLAN_AP_CONNECTED    5
#define WLAN_AP_DISCONNECTED 6
#define WLAN_SCAN_STARTED    7
#define WLAN_SCAN_STOPPED    8

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

typedef struct {
    uint8_t ssid[33];
    int8_t  rssi;
} wlan_ap_t;

/********************
***** FUNCTIONS *****
********************/

void        wlan_init(void);
msg_type_t  wlan_msg_type(void);
void        wlan_toggle_mode(void);
bool        wlan_get_scan_result(uint8_t *cnt, wlan_ap_t **ap);
void        wlan_free_scan_result(void);
