#pragma once

#include <cJSON.h>
#include <stdbool.h>
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

typedef struct {
    char *method;
    cJSON *params;
    uint32_t id;
} json_rpc_request_t;

/********************
***** FUNCTIONS *****
********************/

void json_rpc_init(void);
bool json_rpc_parse_request(const char *text, json_rpc_request_t **request, char **error);
void json_rpc_build_response(cJSON *result, /*uint32_t id,*/ char **response);
void json_rpc_build_error_method_not_found(uint32_t id, char **error);
void json_rpc_build_error_invalid_params(uint32_t id, char **error);
void json_rpc_build_error(uint8_t code, /*uint32_t id,*/ char **error);
