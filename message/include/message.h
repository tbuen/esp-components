#pragma once

#include <stdint.h>

/********************
***** CONSTANTS *****
********************/

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

typedef uint8_t msg_handle_t;
typedef uint8_t msg_type_t;

typedef void (*msg_free_t)(void *ptr);

typedef struct {
    msg_type_t type;
    union {
        uint32_t value;
        struct {
            void *ptr;
            msg_free_t free;
        };
    };
} msg_t;

/********************
***** FUNCTIONS *****
********************/

void            msg_init(void);
msg_type_t      msg_register(void);
msg_handle_t    msg_listen(msg_type_t msg_types);
void            msg_send_value(msg_type_t msg_type, uint32_t value);
void            msg_send_ptr(msg_type_t msg_type, void *ptr, msg_free_t free);
void            msg_free(msg_t *msg);
msg_t           msg_receive(msg_handle_t);
