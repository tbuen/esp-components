#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "filesystem.h"

/***************************
***** CONSTANTS ************
***************************/

#define FILESYSTEM_LABEL         "filesystem"
#define MOUNTPOINT               "/spiflash"
#define WIFI_CFG_FILE            "/spiflash/wificfg.bin"

/***************************
***** MACROS ***************
***************************/

#define TAG "filesystem"

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

static SemaphoreHandle_t mutex;
static fs_wifi_cfg_t wifi_cfg;

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void fs_init(void) {
    mutex = xSemaphoreCreateMutex();
    esp_vfs_fat_mount_config_t fat_config = {
        .format_if_mount_failed = true,
        .max_files = 1,
        .allocation_unit_size = 0
    };
    wl_handle_t wl_handle;
    ESP_ERROR_CHECK(esp_vfs_fat_spiflash_mount_rw_wl(MOUNTPOINT, FILESYSTEM_LABEL, &fat_config, &wl_handle));
    FILE *f = fopen(WIFI_CFG_FILE, "r");
    if (f) {
        fread(&wifi_cfg, sizeof(fs_wifi_cfg_t), 1, f);
        fclose(f);
    } else {
        ESP_LOGW(TAG, "could not open %s", WIFI_CFG_FILE);
    }
}

fs_wifi_cfg_t *fs_get_wifi_cfg(void) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    return &wifi_cfg;
}

void fs_free_wifi_cfg(bool save) {
    if (save) {
        FILE *f = fopen(WIFI_CFG_FILE, "w");
        if (f) {
            fwrite(&wifi_cfg, sizeof(fs_wifi_cfg_t), 1, f);
            fclose(f);
        } else {
            ESP_LOGW(TAG, "could not open %s", WIFI_CFG_FILE);
        }
    }
    xSemaphoreGive(mutex);
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/
