#include <esp_http_server.h>
#include <esp_log.h>
#include <unistd.h>

#include "message.h"
#include "http_srv.h"

/***************************
***** CONSTANTS ************
***************************/

#define MAX_CLIENT_CONNECTIONS  5

/***************************
***** MACROS ***************
***************************/

#define TAG "http"

#define LOGE(...) ESP_LOGE(TAG, __VA_ARGS__)
#define LOGW(...) ESP_LOGW(TAG, __VA_ARGS__)
#define LOGI(...) ESP_LOGI(TAG, __VA_ARGS__)
#define LOGD(...) ESP_LOGD(TAG, __VA_ARGS__)

/***************************
***** TYPES ****************
***************************/

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static void close_fn(httpd_handle_t hd, int sockfd);
static esp_err_t websocket_handler(httpd_req_t *req);
static void free_ws_msg(void *ptr);

/***************************
***** LOCAL VARIABLES ******
***************************/

static httpd_handle_t server = NULL;

static const httpd_uri_t websocket = {
    .uri = "/websocket",
    .method = HTTP_GET,
    .handler = &websocket_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = false
};

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void http_start(con_mode_t mode) {
    if (server) return;

    con_mode_t *mode_ptr = malloc(sizeof(con_mode_t));
    *mode_ptr = mode;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = MAX_CLIENT_CONNECTIONS;
    config.close_fn = &close_fn;
    config.global_user_ctx = mode_ptr;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &websocket);
    }

    if (server) {
        LOGI("started");
    } else {
        LOGE("failed to start");
    }
}

void http_stop(void) {
    if (!server) return;

    size_t fds = MAX_CLIENT_CONNECTIONS;
    int client_fds[MAX_CLIENT_CONNECTIONS];
    ESP_ERROR_CHECK(httpd_get_client_list(server, &fds, client_fds));

    for (size_t i = 0; i < fds; ++i) {
        if (httpd_ws_get_fd_info(server, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_frame_t ws_pkt = {
                .final = true,
                .fragmented = false,
                .type = HTTPD_WS_TYPE_CLOSE,
                .payload = NULL,
                .len = 0
            };
            httpd_ws_send_data(server, client_fds[i], &ws_pkt);
        }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    httpd_stop(server);
    server = NULL;

    LOGI("stopped");
}

void ws_send(con_id_t con, const char *text) {
    if (!server) return;

    int sockfd;
    if (con_get_sock(con, &sockfd)) {
        if (httpd_ws_get_fd_info(server, sockfd) != HTTPD_WS_CLIENT_WEBSOCKET) return;

        httpd_ws_frame_t ws_pkt = {
            .final = true,
            .fragmented = false,
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t*)text,
            .len = strlen(text)
        };
        httpd_ws_send_data(server, sockfd, &ws_pkt);
    }
}

/***************************
***** LOCAL FUNCTIONS ******
***************************/

static void close_fn(httpd_handle_t hd, int sockfd) {
    LOGI("close socket %d", sockfd);
    close(sockfd);
    con_delete(sockfd);
}

static esp_err_t websocket_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        con_mode_t mode = *(con_mode_t*)httpd_get_global_user_ctx(req->handle);
        con_create(mode, httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        LOGE("httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    LOGD("packet type: %d len: %d", ws_pkt.type, ws_pkt.len);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        if (ws_pkt.len) {
            buf = calloc(1, ws_pkt.len + 1);
            if (buf == NULL) {
                LOGE("Failed to calloc memory for buf");
                return ESP_ERR_NO_MEM;
            }
            ws_pkt.payload = buf;
            ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
            if (ret != ESP_OK) {
                LOGE("httpd_ws_recv_frame failed with %d", ret);
                free(buf);
                return ret;
            }

            con_id_t con;
            if (con_get_con(httpd_req_to_sockfd(req), &con)) {
                ws_msg_t *ws_msg = calloc(1, sizeof(ws_msg_t));
                ws_msg->con = con;
                ws_msg->text = (char*)buf;
                msg_send_ptr(MSG_WS_RECV, ws_msg, &free_ws_msg);
            }
        }
    }
    return ret;
}

static void free_ws_msg(void *ptr) {
    ws_msg_t *ws_msg = ptr;
    free(ws_msg->text);
}
