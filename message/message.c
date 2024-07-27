#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "message.h"

/***************************
***** CONSTANTS ************
***************************/

#define MAX_HANDLES     20
#define MAX_MESSAGES    10

/***************************
***** MACROS ***************
***************************/

#define TAG "msg"

#define LOGE(...) ESP_LOGE(TAG, __VA_ARGS__)
#define LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#define LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#define LOGD(...) ESP_LOGD(TAG, __VA_ARGS__)

/***************************
***** TYPES ****************
***************************/

typedef struct {
    msg_type_t types;
    QueueHandle_t queue;
} receiver_t;

/***************************
***** LOCAL FUNCTIONS ******
***************************/

/***************************
***** LOCAL VARIABLES ******
***************************/

static receiver_t receiver[MAX_HANDLES];
static uint8_t next_type;
static msg_handle_t next_handle;
static SemaphoreHandle_t mutex;

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void msg_init(void) {
    mutex = xSemaphoreCreateMutex();
}

msg_type_t msg_register(void) {
    assert(next_type < sizeof(msg_type_t) * 8);
    msg_type_t type = 1 << next_type++;
    return type;
}

msg_handle_t msg_listen(msg_type_t msg_types) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    assert(next_handle < MAX_HANDLES);
    msg_handle_t handle = next_handle++;
    receiver[handle].types = msg_types;
    receiver[handle].queue = xQueueCreate(MAX_MESSAGES, sizeof(msg_t));
    xSemaphoreGive(mutex);
    return handle;
}

void msg_send_value(msg_type_t msg_type, uint32_t value) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    msg_t msg = {
        .type = msg_type,
        .value = value
    };
    for (int i = 0; i < next_handle; ++i) {
        if (receiver[i].types & msg_type) {
            assert(xQueueSendToBack(receiver[i].queue, &msg, 0) == pdPASS);
        }
    }
    xSemaphoreGive(mutex);
}

void msg_send_ptr(msg_type_t msg_type, void *ptr, msg_free_t free) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    msg_t msg = {
        .type = msg_type,
        .ptr = ptr,
        .free = free
    };
    for (int i = 0; i < next_handle; ++i) {
        if (receiver[i].types & msg_type) {
            assert(xQueueSendToBack(receiver[i].queue, &msg, 0) == pdPASS);
        }
    }
    xSemaphoreGive(mutex);
}

void msg_free(msg_t *msg) {
    assert(msg->ptr);
    if (msg->free) {
        msg->free(msg->ptr);
    }
    free(msg->ptr);
    msg->ptr = NULL;
}

msg_t msg_receive(msg_handle_t handle) {
    assert(handle < MAX_HANDLES);
    msg_t msg;
    assert(xQueueReceive(receiver[handle].queue, &msg, portMAX_DELAY) == pdPASS);
    return msg;
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/
