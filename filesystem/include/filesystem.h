#pragma once

#include <cJSON.h>
#include <stdbool.h>
#include <stdint.h>

/********************
***** CONSTANTS *****
********************/

#define FS_NUMBER_OF_WEB_FILES      10
#define FS_MAX_FILENAME_LEN         32

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

typedef struct fs_web_file {
    char name[FS_MAX_FILENAME_LEN + 1];
    char *content_type;
    uint32_t size;
    uint8_t md5[16];
} fs_web_file_t;

typedef struct {
    uint64_t total;
    uint64_t free;
    uint8_t num_files;
    fs_web_file_t files[FS_NUMBER_OF_WEB_FILES];
} fs_web_info_t;

typedef enum {
    FS_WEB_READ,
    FS_WEB_WRITE
} fs_mode_t;

/********************
***** FUNCTIONS *****
********************/

void     fs_init(void);
cJSON   *fs_get_wifi_cfg(void);
void     fs_free_wifi_cfg(bool save);
void     fs_web_info(fs_web_info_t *info);
bool     fs_web_exist(const char *filename);
int      fs_web_open(const char *filename, fs_mode_t mode, char **content_type);
int16_t  fs_web_read(int fd, char *data, int16_t len);
int16_t  fs_web_write(int fd, const char *data, int16_t len);
void     fs_web_close(int fd);
void     fs_web_delete(const char *filename);
