#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/********************
***** CONSTANTS *****
********************/

#define MSG_CON     0x0001

#define CON_CONNECTED    1
#define CON_DISCONNECTED 2

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

typedef enum {
    CON_AP,
    CON_STA
} con_mode_t;

typedef uint32_t con_id_t;

/********************
***** FUNCTIONS *****
********************/

void con_init(void);
void con_create(con_mode_t mode, int sockfd);
void con_delete(int sockfd);
size_t con_count(void);
bool con_get_con(int sockfd, con_id_t *con);
bool con_get_sock(con_id_t con, int *sockfd);
