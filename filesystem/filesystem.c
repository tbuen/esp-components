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

typedef struct {
    char *extension;
    char *content_type;
} content_type_mapping_t;

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static bool fs_check_filename(const char *filename);
static char *fs_get_content_type(const char *filename);

/***************************
***** LOCAL VARIABLES ******
***************************/

static SemaphoreHandle_t mutex;
static fs_wifi_cfg_t wifi_cfg;
static FILE *fd_table[MAX_FILES_OPEN];
static content_type_mapping_t content_type_mapping[] = {
    { ".html", "text/html"              },
    { ".css" , "text/css"               },
    { ".js"  , "application/javascript" },
    { ".png" , "image/png"              },
};

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
    if (stat(WEB_DIR, &st)) {
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

void fs_web_info(fs_web_info_t *info) {
    memset(info, 0, sizeof(fs_web_info_t));
    DIR *dir = opendir(WEB_DIR);
    if (dir) {
        char full_name[sizeof(WEB_DIR) + 1 + FS_MAX_FILENAME_LEN];
        struct stat st;
        struct dirent *entry= readdir(dir);
        while (entry && (info->num_files < FS_NUMBER_OF_WEB_FILES)) {
            if (entry->d_type == DT_REG) {
                strncpy(info->files[info->num_files].name, entry->d_name, FS_MAX_FILENAME_LEN);
                info->files[info->num_files].content_type = fs_get_content_type(entry->d_name);
                sprintf(full_name, "%s/%.*s", WEB_DIR, FS_MAX_FILENAME_LEN, entry->d_name);
                if (!stat(full_name, &st)) {
                    info->files[info->num_files].size = st.st_size;
                }
                info->num_files++;
            }
            entry = readdir(dir);
        }
        closedir(dir);
    }
    ESP_ERROR_CHECK(esp_vfs_fat_info(MOUNTPOINT, &info->total, &info->free));
}

bool fs_web_exist(const char *filename) {
    bool ret = false;
    if (fs_check_filename(filename)) {
        struct stat st;
        char full_name[sizeof(WEB_DIR) + 1 + FS_MAX_FILENAME_LEN];
        sprintf(full_name, "%s%s", WEB_DIR, filename);
        if (!stat(full_name, &st)) {
            ret = true;
        }
    }
    return ret;
}

int fs_web_open(const char *filename, fs_mode_t mode, char **content_type) {
    int ret = -1;
    if (fs_check_filename(filename)) {
        if (content_type) {
            *content_type = fs_get_content_type(filename);
        }
        for (int i = 0; i < MAX_FILES_OPEN; ++i) {
            if (!fd_table[i]) {
                char full_name[sizeof(WEB_DIR) + 1 + FS_MAX_FILENAME_LEN];
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

int16_t fs_web_read(int fd, char *data, int16_t len) {
    int16_t ret = 0;
    if (len > 0) {
        if (   (fd >= 0)
            && (fd < MAX_FILES_OPEN)
            && (fd_table[fd]))
        {
            ret = fread(data, 1, len, fd_table[fd]);
        } else {
           ret = -1;
        }
    }
    return ret;
}

int16_t fs_web_write(int fd, const char *data, int16_t len) {
    int16_t ret = 0;
    if (len > 0) {
        if (   (fd >= 0)
            && (fd < MAX_FILES_OPEN)
            && (fd_table[fd]))
        {
            ret = fwrite(data, 1, len, fd_table[fd]);
        } else {
           ret = -1;
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
        char full_name[sizeof(WEB_DIR) + 1 + FS_MAX_FILENAME_LEN];
        sprintf(full_name, "%s%s", WEB_DIR, filename);
        remove(full_name);
    }
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static bool fs_check_filename(const char *filename) {
    bool ret = false;
    if (filename && (filename[0] == '/') && filename[1] && (strlen(&filename[1]) <= FS_MAX_FILENAME_LEN)) {
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

static char *fs_get_content_type(const char *filename) {
   char *content_type = "";
   char *ext = strrchr(filename, '.');
   if (ext) {
       for (int i = 0; i < sizeof(content_type_mapping)/sizeof(content_type_mapping[0]); ++i) {
           if (!strcmp(ext, content_type_mapping[i].extension)) {
               content_type = content_type_mapping[i].content_type;
               break;
           }
       }
   }
   return content_type;
}
