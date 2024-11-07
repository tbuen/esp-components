#pragma once

#include <cJSON.h>

/********************
***** CONSTANTS *****
********************/

/********************
***** MACROS ********
********************/

/********************
***** TYPES *********
********************/

typedef void (*json_rpc_handler_t)(void *params, void **result);
typedef void *(*json_rpc_param_parser_t)(cJSON *params);
typedef cJSON *(*json_rpc_result_builder_t)(void *result);

typedef struct {
    char                      *method;
    json_rpc_handler_t        handler;
    json_rpc_param_parser_t   param_parser;
    json_rpc_result_builder_t result_builder;
} json_rpc_config_t;

/********************
***** FUNCTIONS *****
********************/

void json_rpc_init(const json_rpc_config_t *cfg);
char *json_rpc_handle_request(const char *request);
