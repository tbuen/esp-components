#pragma once

/********************
***** CONSTANTS *****
********************/

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

typedef enum {
    LED_RED,
    LED_GREEN,
    LED_YELLOW,
    LED_BLUE
} led_color_t;

typedef enum {
    LED_OFF,
    LED_LOW,
    LED_HIGH,
} led_state_t;

/********************
***** FUNCTIONS *****
********************/

void led_init(void);
void led_set(led_color_t color, led_state_t state);
