#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#include "filesystem.h"

/***************************
***** CONSTANTS ************
***************************/

#define FILESYSTEM_LABEL        "filesystem"
#define MOUNTPOINT              "/spiflash"
#define WIFI_CFG_FILE           "/spiflash/wificfg.bin"
#define WEB_DIR                 "/spiflash/web"
#define MAX_FILES_OPEN          5
#define MAX_FILENAME_LEN        33

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

static bool fs_check_filename(const char *filename);

/***************************
***** LOCAL VARIABLES ******
***************************/

static SemaphoreHandle_t mutex;
static fs_wifi_cfg_t wifi_cfg;
static FILE *fd_table[MAX_FILES_OPEN];

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
        LOGW("could not open %s", WIFI_CFG_FILE);
    }
    struct stat st;
    if (stat(WEB_DIR, &st) == 0) {
        DIR *dir = opendir(WEB_DIR);
        if (dir) {
            struct dirent *entry= readdir(dir);
            while (entry) {
                if (entry->d_type == DT_REG) {
                    LOGI("WEB file: %s", entry->d_name);
                }
                entry = readdir(dir);
            }
            closedir(dir);
        }
    } else {
        LOGW("creating %s", WEB_DIR);
        mkdir(WEB_DIR, 0);
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
            LOGW("could not open %s", WIFI_CFG_FILE);
        }
    }
    xSemaphoreGive(mutex);
}

bool fs_web_exist(const char *filename) {
    bool ret = false;
    if (fs_check_filename(filename)) {
        struct stat st;
        char full_name[sizeof(WEB_DIR) + MAX_FILENAME_LEN];
        sprintf(full_name, "%s%s", WEB_DIR, filename);
        if (!stat(full_name, &st)) {
            ret = true;
        }
    }
    return ret;
}

int fs_web_open(const char *filename, fs_mode_t mode) {
    int ret = -1;
    if (fs_check_filename(filename)) {
        for (int i = 0; i < MAX_FILES_OPEN; ++i) {
            if (!fd_table[i]) {
                char full_name[sizeof(WEB_DIR) + MAX_FILENAME_LEN];
                sprintf(full_name, "%s%s", WEB_DIR, filename);
                fd_table[i] = fopen(full_name, mode == FS_WEB_WRITE ? "w" : "r");
                if (fd_table[i]) {
                    ret = i;
                }
                break;
            }
        }
    }
    return ret;
}

void fs_web_close(int fd) {
    if (   (fd >= 0)
        && (fd < MAX_FILES_OPEN)
        && (fd_table[fd]))
    {
        fclose(fd_table[fd]);
        fd_table[fd] = NULL;
    }
}

void fs_web_delete(const char *filename) {
    if (fs_check_filename(filename)) {
        char full_name[sizeof(WEB_DIR) + MAX_FILENAME_LEN];
        sprintf(full_name, "%s%s", WEB_DIR, filename);
        remove(full_name);
    }
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static bool fs_check_filename(const char *filename) {
    bool ret = false;
    if (filename && (filename[0] == '/') && filename[1] && (strlen(filename) <= MAX_FILENAME_LEN)) {
        ret = true;
        const char *x = &filename[1];
        while (*x) {
            if (   (*x < 'A' || *x > 'Z')
                && (*x < 'a' || *x > 'z')
                && (*x < '0' || *x > '9')
                && (*x != '-')
                && (*x != '_')
                && (*x != '.'))
            {
                ret = false;
                break;
            }
            x++;
        }
    }
    return ret;
}
