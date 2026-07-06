#include <driver/gpio.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "defs.h"
#include "sdi.h"

/***************************
***** CONSTANTS ************
***************************/

/***************************
***** MACROS ***************
***************************/

/***************************
***** TYPES ****************
***************************/

/***************************
***** LOCAL FUNCTIONS ******
***************************/

/***************************
***** LOCAL VARIABLES ******
***************************/

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void sdi_write(spi_device_handle_t handle, const uint8_t *data, uint16_t len) {
    while (len > 0) {
        uint16_t l = len;
        if (l > 32) {
            l = 32;
        }
        while (gpio_get_level(PIN_DREQ) != 1) {
            vTaskDelay(1);
            //taskYIELD();
        }
        spi_transaction_t trans = {
            .length = l * 8,
            .tx_buffer = data,
        };
        ESP_ERROR_CHECK(spi_device_transmit(handle, &trans));
        data += l;
        len -= l;
    }
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/
