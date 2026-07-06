#pragma once

#include <driver/spi_master.h>

void     sci_write(spi_device_handle_t handle, uint8_t addr, uint16_t data);
uint16_t sci_read(spi_device_handle_t handle, uint8_t addr);
