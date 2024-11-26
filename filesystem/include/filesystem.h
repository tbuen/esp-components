#pragma once

#include <stdbool.h>
#include <stdint.h>

/********************
***** CONSTANTS *****
********************/

#define FS_NUMBER_OF_WIFI_NETWORKS  5

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

typedef struct {
    uint8_t ssid[32];
    uint8_t key[64];
} fs_wifi_network_t;

typedef struct {
    fs_wifi_network_t network[FS_NUMBER_OF_WIFI_NETWORKS];
} fs_wifi_cfg_t;

typedef enum {
    FS_WEB_READ,
    FS_WEB_WRITE
} fs_mode_t;

/********************
***** FUNCTIONS *****
********************/

void            fs_init(void);
fs_wifi_cfg_t  *fs_get_wifi_cfg(void);
void            fs_free_wifi_cfg(bool save);
bool            fs_web_exist(const char *filename);
int             fs_web_open(const char *filename, fs_mode_t mode);
void            fs_web_close(int fd);
void            fs_web_delete(const char *filename);
