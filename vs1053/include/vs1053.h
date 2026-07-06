#pragma once

#include <esp_err.h>

/********************
***** CONSTANTS *****
********************/

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

/********************
***** FUNCTIONS *****
********************/

esp_err_t   vs_init(void);
uint16_t    vs_get_volume(void);
void        vs_set_volume(uint16_t vol);
void        vs_send_data(const uint8_t *data, uint16_t len);
const char *vs_get_status(void);
esp_err_t   vs_card_init(const char *mountpoint);
