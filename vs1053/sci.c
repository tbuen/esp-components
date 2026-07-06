#include <driver/gpio.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "defs.h"
#include "sci.h"

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

void sci_write(spi_device_handle_t handle, uint8_t addr, uint16_t data) {
    while (gpio_get_level(PIN_DREQ) != 1) {
        taskYIELD();
    }
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .cmd = CMD_WRITE,
        .addr = addr,
        .length = 16,
        .tx_data = {data >> 8, data & 0xFF},
    };
    ESP_ERROR_CHECK(spi_device_transmit(handle, &trans));
}

uint16_t sci_read(spi_device_handle_t handle, uint8_t addr) {
    while (gpio_get_level(PIN_DREQ) != 1) {
        taskYIELD();
    }
    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_RXDATA,
        .cmd = CMD_READ,
        .addr = addr,
        .rxlength = 16,
    };
    ESP_ERROR_CHECK(spi_device_transmit(handle, &trans));
    return trans.rx_data[0] << 8 | trans.rx_data[1];
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/
