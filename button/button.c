#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "button.h"

/***************************
***** CONSTANTS ************
***************************/

#define TASK_CORE                 1
#define TASK_PRIO                 1
#define STACK_SIZE             4096

#define BUTTON_PIN      GPIO_NUM_22
#define BUTTON_ACTIVE             0
#define BUTTON_DELAY_MS         100

/***************************
***** MACROS ***************
***************************/

#define TAG "button"

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

static void button_task(void *param);

/***************************
***** LOCAL VARIABLES ******
***************************/

static TaskHandle_t handle;
static msg_type_t   msg_type;

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void button_init(void) {
    assert(!handle);
    assert(!msg_type);

    msg_type = msg_register();

    gpio_config_t io_conf = {};

    io_conf.pin_bit_mask = BIT64(BUTTON_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    if (xTaskCreatePinnedToCore(&button_task, "button-task", STACK_SIZE, NULL, TASK_PRIO, &handle, TASK_CORE) != pdPASS) {
        LOGE("could not create task");
    }
}

msg_type_t button_msg_type(void) {
    assert(msg_type);
    return msg_type;
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static void button_task(void *param) {
    uint8_t button_cnt = 0;
    for (;;) {
        if (gpio_get_level(BUTTON_PIN) == BUTTON_ACTIVE) {
            if (button_cnt < BUTTON_DELAY_MS / 10) {
                button_cnt++;
                if (button_cnt == BUTTON_DELAY_MS / 10) {
                    LOGI("BUTTON pressed!");
                    msg_send_value(msg_type, BUTTON_PRESSED);
                }
            }
        } else {
            button_cnt = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
