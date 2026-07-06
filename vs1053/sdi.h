#pragma once

#include <driver/spi_master.h>

void sdi_write(spi_device_handle_t handle, const uint8_t *data, uint16_t len);
