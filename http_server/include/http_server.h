#pragma once

#include "message.h"
#include "connection.h"

/********************
***** CONSTANTS *****
********************/

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

typedef struct {
    con_id_t con;
    char *text;
} ws_msg_t;

/********************
***** FUNCTIONS *****
********************/

void        http_init(void);
void        http_start(con_mode_t mode);
void        http_stop(void);
msg_type_t  http_msg_type_ws_recv(void);
