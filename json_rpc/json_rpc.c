#include <stdint.h>
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

static char *json_rpc_build_response(cJSON *result, int id);
static char *json_rpc_build_error_msg(int16_t code, int *id);
static char *json_rpc_error_message(int16_t code);

/***************************
***** LOCAL VARIABLES ******
***************************/

static const json_rpc_config_t *config;
static const json_rpc_error_config_t *error_config;

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void json_rpc_init(const json_rpc_config_t *cfg, const json_rpc_error_config_t *err_cfg) {
    config = cfg;
    error_config = err_cfg;
}

char *json_rpc_handle_request(const char *request) {
    assert(config);

    int *idptr = NULL;
    char *response = NULL;

    cJSON *req = cJSON_Parse(request);

    if (req) {
        cJSON *jsonrpc = cJSON_GetObjectItemCaseSensitive(req, "jsonrpc");
        cJSON *method = cJSON_GetObjectItemCaseSensitive(req, "method");
        cJSON *params = cJSON_GetObjectItemCaseSensitive(req, "params");
        cJSON *id = cJSON_GetObjectItemCaseSensitive(req, "id");
        if (cJSON_IsNumber(id)) {
            idptr = &id->valueint;
        }
        if (   cJSON_IsString(jsonrpc)
            && !strcmp(jsonrpc->valuestring, "2.0")
            && cJSON_IsString(method)
            && (!params || cJSON_IsArray(params) || cJSON_IsObject(params))
            && idptr) {
            const json_rpc_config_t *cfg = config;
            while (cfg->method) {
                if (!strcmp(method->valuestring, cfg->method)) {
                    break;
                }
                cfg++;
            }
            if (cfg->method && cfg->handler && cfg->result_builder) {
                void *parameters = NULL;
                if (cfg->param_parser && params) {
                    if (!(parameters = cfg->param_parser(params))) {
                        response = json_rpc_build_error_msg(JSON_RPC_INVALID_PARAMS, idptr);
                    }
                } else if (cfg->param_parser || params) {
                    response = json_rpc_build_error_msg(JSON_RPC_INVALID_PARAMS, idptr);
                }
                if (!response) {
                    void *result = NULL;
                    cJSON *json_result = NULL;
                    cfg->handler(parameters, &result);
                    uint8_t error = cfg->result_builder(result, &json_result);
                    if (error) {
                        response = json_rpc_build_error_msg(error, idptr);
                    } else {
                        response = json_rpc_build_response(json_result, *idptr);
                    }
                }
            } else {
                response = json_rpc_build_error_msg(JSON_RPC_METHOD_NOT_FOUND, idptr);
            }
        } else {
            response = json_rpc_build_error_msg(JSON_RPC_INVALID_REQUEST, idptr);
        }
        cJSON_Delete(req);
    } else {
        response = json_rpc_build_error_msg(JSON_RPC_PARSE_ERROR, idptr);
    }
    return response;
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static char *json_rpc_build_response(cJSON *result, int id) {
    cJSON *resp = cJSON_CreateObject();

    cJSON_AddStringToObject(resp, "jsonrpc", "2.0");
    cJSON_AddItemToObject(resp, "result", result);
    cJSON_AddNumberToObject(resp, "id", id);

    char *response = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    return response;
}

static char *json_rpc_build_error_msg(int16_t code, int *id) {
    cJSON *resp = cJSON_CreateObject();
    cJSON *err = cJSON_CreateObject();

    cJSON_AddStringToObject(resp, "jsonrpc", "2.0");
    cJSON_AddItemToObject(resp, "error", err);
    cJSON_AddNumberToObject(err, "code", code);
    cJSON_AddStringToObject(err, "message", json_rpc_error_message(code));

    if (id) {
        cJSON_AddNumberToObject(resp, "id", *id);
    } else {
        cJSON_AddNullToObject(resp, "id");
    }

    char *response = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    return response;
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
            const json_rpc_error_config_t *err = error_config;
            while (err && err->code) {
                if (code == err->code) {
                    ret = err->message;
                    break;
                }
                err++;
            }
            break;
    }
    return ret;
}
