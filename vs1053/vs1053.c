#include <diskio_impl.h>
#include <diskio_sdmmc.h>
#include <driver/gpio.h>
#include <driver/sdspi_host.h>
#include <driver/sdmmc_host.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <esp_check.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdmmc_cmd.h>
#include <string.h>

#include "defs.h"
#include "sci.h"
#include "sdi.h"
#include "vs1053.h"

/***************************
***** CONSTANTS ************
***************************/

/***************************
***** MACROS ***************
***************************/

#define TAG "vs1053"

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

/***************************
***** LOCAL VARIABLES ******
***************************/

static spi_device_handle_t sci_handle;
static spi_device_handle_t sdi_handle;
static bool initialized;
static sdmmc_card_t card;
static char *mounted;
static char statusbuf[32];

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

esp_err_t vs_init(void) {
    if (initialized) return ESP_OK;

    // configure pins
    gpio_config_t io_conf = {};

    io_conf.pin_bit_mask = BIT64(PIN_RESET);
    io_conf.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    io_conf.pin_bit_mask = BIT64(PIN_DREQ);
    io_conf.mode = GPIO_MODE_INPUT;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // reset active
    gpio_set_level(PIN_RESET, 0);

    // init spi
    spi_bus_config_t bus_conf = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &bus_conf, SPI_DMA_CH1));

    // add sci device to spi bus
    spi_device_interface_config_t sci_conf = {
        .command_bits = 8,
        .address_bits = 8,
        .clock_speed_hz = SCI_FREQ,
        .spics_io_num = PIN_CS_SCI,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(HOST, &sci_conf, &sci_handle));

    // add sdi device to spi bus
    spi_device_interface_config_t sdi_conf = {
        .command_bits = 0,
        .address_bits = 0,
        .clock_speed_hz = SDI_FREQ,
        .spics_io_num = PIN_CS_SDI,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(HOST, &sdi_conf, &sdi_handle));

    // add sd card device to spi bus
    sdspi_device_config_t sd_conf = {
        .host_id = HOST,
        .gpio_cs = PIN_CS_SD,
        .gpio_cd = SDSPI_SLOT_NO_CD,
        .gpio_wp = SDSPI_SLOT_NO_WP,
        .gpio_int = SDSPI_SLOT_NO_INT,
    };
    sdspi_dev_handle_t sd_handle;
    ESP_ERROR_CHECK(sdspi_host_init_device(&sd_conf, &sd_handle));

    // wake from reset
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    sci_write(sci_handle, REG_MODE, MODE_SDINEW | MODE_RESET);
    vTaskDelay(pdMS_TO_TICKS(100));

    // set clock frequency
    sci_write(sci_handle, REG_CLOCKF, 0x6000); // MULT 3.0 ADD 0.0

    // read status
    uint16_t status = sci_read(sci_handle, REG_STATUS);
    LOGD("status: 0x%04x", status);
    if (((status & 0xF0) >> 4) == 4) {
        LOGI("init ok");
        initialized = true;
        sci_write(sci_handle, REG_VOL, 0xFEFE);
    } else {
        LOGE("invalid version");
    }

    return initialized ? ESP_OK : ESP_FAIL;
}

uint16_t vs_get_volume(void) {
    if (!initialized) return 0xFFFF;

    uint16_t vol = sci_read(sci_handle, REG_VOL);
    LOGI("volume get %1.1fdB %1.1fdB", -0.5 * (vol >> 8), -0.5 * (vol & 0xFF));
    return vol;
}

void vs_set_volume(uint16_t vol) {
    if (!initialized) return;

    LOGI("volume set %1.1fdB %1.1fdB", -0.5 * (vol >> 8), -0.5 * (vol & 0xFF));
    sci_write(sci_handle, REG_VOL, vol);
}

void vs_send_data(const uint8_t *data, uint16_t len) {
    if (!initialized) return;

    sdi_write(sdi_handle, data, len);
}

const char *vs_get_status(void) {
    uint16_t hdat0 = sci_read(sci_handle, REG_HDAT0);
    uint16_t hdat1 = sci_read(sci_handle, REG_HDAT1);
    sprintf(statusbuf, "0x%04x 0x%04x", hdat0, hdat1);
    return statusbuf;
}

esp_err_t vs_card_init(const char *mountpoint) {
    if (!initialized) return ESP_FAIL;

    const char drive[] = { '0' + FS_NUM, ':', 0 };

    if (mounted) {
        f_unmount(drive);
        ff_diskio_register(FS_NUM, NULL);
        esp_vfs_fat_unregister_path(mounted);
        free(mounted);
        mounted = NULL;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    if (   (sdmmc_card_init(&host, &card) != ESP_OK)
        || (sdmmc_get_status(&card) != ESP_OK)) {
        LOGW("no card");
        return ESP_FAIL;
    }

    esp_vfs_fat_conf_t fatcfg = {
        mountpoint,
        drive,
        MAX_OPEN_FILES
    };
    FATFS *fatfs = NULL;
    ESP_ERROR_CHECK(esp_vfs_fat_register(&fatcfg, &fatfs));

    ff_diskio_register_sdmmc(FS_NUM, &card);

    FRESULT fres = f_mount(fatfs, drive, 1);
    if (fres != FR_OK) {
        LOGE("mount failed: %d", fres);
        ff_diskio_register(FS_NUM, NULL);
        esp_vfs_fat_unregister_path(mountpoint);
        return ESP_FAIL;
    }

    LOGI("card mounted on %s", mountpoint);

    mounted = strdup(mountpoint);

    return ESP_OK;
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/
