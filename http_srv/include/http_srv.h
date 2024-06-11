#pragma once

#include "connection.h"

/********************
***** CONSTANTS *****
********************/

#define MSG_WS_RECV 0x0010

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

void http_start(con_mode_t mode);
void http_stop(void);
