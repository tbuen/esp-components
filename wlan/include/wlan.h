#pragma once

/********************
***** CONSTANTS *****
********************/

#define MSG_WLAN        0x0002
#define MSG_WLAN_INT    0x0004

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

void wlan_init(void);
