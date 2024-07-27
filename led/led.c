#include <driver/ledc.h>

#include "led.h"

/***************************
***** CONSTANTS ************
***************************/

#define LED_MODE           LEDC_LOW_SPEED_MODE
#define LED_TIMER          LEDC_TIMER_0
#define LED_RESOLUTION     LEDC_TIMER_13_BIT
#define LED_FREQUENCY      (5000)
#define LED_DUTY_OFF       (0)
#define LED_DUTY_MAX       (8191)
#define LED_CHANNEL_RED    LEDC_CHANNEL_0
#define LED_CHANNEL_GREEN  LEDC_CHANNEL_1
#define LED_CHANNEL_YELLOW LEDC_CHANNEL_2
#define LED_CHANNEL_BLUE   LEDC_CHANNEL_3
#define LED_PIN_RED        GPIO_NUM_16
#define LED_PIN_GREEN      GPIO_NUM_17
#define LED_PIN_YELLOW     GPIO_NUM_18
#define LED_PIN_BLUE       GPIO_NUM_19

/***************************
***** MACROS ***************
***************************/

/***************************
***** TYPES ****************
***************************/

typedef struct {
    ledc_channel_t ch;
    uint16_t duty[3];
} led_cfg_t;

/***************************
***** LOCAL FUNCTIONS ******
***************************/

/***************************
***** LOCAL VARIABLES ******
***************************/

static led_cfg_t led_cfg[] = { { LED_CHANNEL_RED,    { 0, 200, 1600 } },
                               { LED_CHANNEL_GREEN,  { 0, 100,  800 } },
                               { LED_CHANNEL_YELLOW, { 0, 200, 1600 } },
                               { LED_CHANNEL_BLUE,   { 0, 300, 2400 } } };

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void led_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LED_MODE,
        .timer_num        = LED_TIMER,
        .duty_resolution  = LED_RESOLUTION,
        .freq_hz          = LED_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel_red = {
        .speed_mode     = LED_MODE,
        .channel        = LED_CHANNEL_RED,
        .timer_sel      = LED_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED_PIN_RED,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_red));

    ledc_channel_config_t ledc_channel_green = {
        .speed_mode     = LED_MODE,
        .channel        = LED_CHANNEL_GREEN,
        .timer_sel      = LED_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED_PIN_GREEN,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_green));

    ledc_channel_config_t ledc_channel_yellow = {
        .speed_mode     = LED_MODE,
        .channel        = LED_CHANNEL_YELLOW,
        .timer_sel      = LED_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED_PIN_YELLOW,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_yellow));

    ledc_channel_config_t ledc_channel_blue = {
        .speed_mode     = LED_MODE,
        .channel        = LED_CHANNEL_BLUE,
        .timer_sel      = LED_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LED_PIN_BLUE,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_blue));
}

void led_set(led_color_t color, led_state_t state) {
    ESP_ERROR_CHECK(ledc_set_duty(LED_MODE, led_cfg[color].ch, led_cfg[color].duty[state]));
    ESP_ERROR_CHECK(ledc_update_duty(LED_MODE, led_cfg[color].ch));
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/
