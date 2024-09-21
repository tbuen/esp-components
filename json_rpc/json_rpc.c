#include <string.h>

#include "json_rpc.h"

/***************************
***** CONSTANTS ************
***************************/

#define JSON_RPC_PARSE_ERROR        -32700
#define JSON_RPC_INVALID_REQUEST    -32600
#define JSON_RPC_METHOD_NOT_FOUND   -32601
#define JSON_RPC_INVALID_PARAMS     -32602
#define JSON_RPC_INTERNAL_ERROR     -32603

/***************************
***** MACROS ***************
***************************/

/***************************
***** TYPES ****************
***************************/

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static char *json_rpc_build_error_msg(int16_t code, uint32_t *rpc_id);
static char *json_rpc_error_message(int16_t code);

/***************************
***** LOCAL VARIABLES ******
***************************/

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

bool json_rpc_parse_request(const char *text, json_rpc_request_t **request, char **error) {
    bool ret = false;
    uint32_t rpc_id;
    uint32_t *rpc_id_ptr = NULL;

    cJSON *req = cJSON_Parse(text);

    if (req) {
        cJSON *jsonrpc = cJSON_GetObjectItemCaseSensitive(req, "jsonrpc");
        cJSON *method = cJSON_GetObjectItemCaseSensitive(req, "method");
        cJSON *params = cJSON_GetObjectItemCaseSensitive(req, "params");
        cJSON *id = cJSON_GetObjectItemCaseSensitive(req, "id");
        if (cJSON_IsNumber(id)) {
            rpc_id = id->valueint;
            rpc_id_ptr = &rpc_id;
        }
        if (   cJSON_IsString(jsonrpc)
            && !strcmp(jsonrpc->valuestring, "2.0")
            && cJSON_IsString(method)
            && (!params || cJSON_IsArray(params) || cJSON_IsObject(params))
            && rpc_id_ptr) {
            *request = malloc(sizeof(json_rpc_request_t));
            (*request)->method = malloc(strlen(method->valuestring)+1);
            strcpy((*request)->method, method->valuestring);
            (*request)->params = cJSON_DetachItemViaPointer(req, params);
            (*request)->id = rpc_id;
            ret = true;
        } else {
            *error = json_rpc_build_error_msg(JSON_RPC_INVALID_REQUEST, rpc_id_ptr);
        }
        cJSON_Delete(req);
    } else {
        *error = json_rpc_build_error_msg(JSON_RPC_PARSE_ERROR, rpc_id_ptr);
    }

    return ret;
}

void json_rpc_build_error_method_not_found(uint32_t id, char **error) {
    *error = json_rpc_build_error_msg(JSON_RPC_METHOD_NOT_FOUND, &id);
}

void json_rpc_build_error_invalid_params(uint32_t id, char **error) {
    *error = json_rpc_build_error_msg(JSON_RPC_INVALID_PARAMS, &id);
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static char *json_rpc_build_error_msg(int16_t code, uint32_t *rpc_id) {
    cJSON *resp = cJSON_CreateObject();
    cJSON *err = cJSON_CreateObject();

    cJSON_AddStringToObject(resp, "jsonrpc", "2.0");
    cJSON_AddItemToObject(resp, "error", err);
    cJSON_AddNumberToObject(err, "code", code);
    cJSON_AddStringToObject(err, "message", json_rpc_error_message(code));
    if (rpc_id) {
        cJSON_AddNumberToObject(resp, "id", *rpc_id);
    } else {
        cJSON_AddNullToObject(resp, "id");
    }

    char *text = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    return text;
}

static char *json_rpc_error_message(int16_t code) {
    char *ret = "";
    switch (code) {
        case JSON_RPC_PARSE_ERROR:
            ret = "parse error";
            break;
        case JSON_RPC_INVALID_REQUEST:
            ret = "invalid request";
            break;
        case JSON_RPC_METHOD_NOT_FOUND:
            ret = "method not found";
            break;
        case JSON_RPC_INVALID_PARAMS:
            ret = "invalid params";
            break;
        case JSON_RPC_INTERNAL_ERROR:
            ret = "internal error";
            break;
        default:
            break;
    }
    return ret;
}
