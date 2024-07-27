#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include "connection.h"

/***************************
***** CONSTANTS ************
***************************/

#define MAX_CLIENT_CONNECTIONS  5

/***************************
***** MACROS ***************
***************************/

#define TAG "con"

#define LOGE(...) ESP_LOGE(TAG, __VA_ARGS__)
#define LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#define LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#define LOGD(...) ESP_LOGD(TAG, __VA_ARGS__)

/***************************
***** TYPES ****************
***************************/

typedef struct {
    con_id_t con;
    con_mode_t mode;
    int sockfd;
} connection_t;

/***************************
***** LOCAL FUNCTIONS ******
***************************/

/***************************
***** LOCAL VARIABLES ******
***************************/

static msg_type_t           msg_type;
static SemaphoreHandle_t    mutex;
static connection_t         connection[MAX_CLIENT_CONNECTIONS];
static con_id_t             next_con = 1;

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void con_init(void) {
    assert(!msg_type);
    msg_type = msg_register();
    mutex = xSemaphoreCreateMutex();
}

msg_type_t con_msg_type(void) {
    assert(msg_type);
    return msg_type;
}

void con_create(con_mode_t mode, int sockfd) {
    bool created = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < sizeof(connection)/sizeof(connection_t); ++i) {
            if (!connection[i].con) {
                connection[i].con = next_con++;
                connection[i].mode = mode;
                connection[i].sockfd = sockfd;
                LOGI("create con %lu mode %d socket %d", connection[i].con, connection[i].mode, connection[i].sockfd);
                msg_send_value(msg_type, CON_CONNECTED);
                created = true;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }
    if (!created) {
        LOGE("error creating connection");
    }
}

void con_delete(int sockfd) {
    bool deleted = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < sizeof(connection)/sizeof(connection_t); ++i) {
            if (connection[i].con && connection[i].sockfd == sockfd) {
                LOGI("delete con %lu socket %d", connection[i].con, connection[i].sockfd);
                memset(&connection[i], 0, sizeof(connection_t));
                msg_send_value(msg_type, CON_DISCONNECTED);
                deleted = true;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }
    if (!deleted) {
        LOGW("error deleting connection");
    }
}

size_t con_count(void) {
    size_t count = 0;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < sizeof(connection)/sizeof(connection_t); ++i) {
            if (connection[i].con) {
                count++;
            }
        }
        xSemaphoreGive(mutex);
    }
    LOGI("count %d", count);
    return count;
}

bool con_get_con(int sockfd, con_id_t *con) {
    bool found = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < sizeof(connection)/sizeof(connection_t); ++i) {
            if (connection[i].sockfd == sockfd) {
                *con = connection[i].con;
                found = true;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }
    return found;
}

bool con_get_sock(con_id_t con, int *sockfd) {
    bool found = false;
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = 0; i < sizeof(connection)/sizeof(connection_t); ++i) {
            if (connection[i].con == con) {
                *sockfd = connection[i].sockfd;
                found = true;
                break;
            }
        }
        xSemaphoreGive(mutex);
    }
    return found;
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/
