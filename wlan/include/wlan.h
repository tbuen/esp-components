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

/********************
***** FUNCTIONS *****
********************/

void        wlan_init(void);
msg_type_t  wlan_msg_type(void);
void        wlan_toggle_mode(void);
