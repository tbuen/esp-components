#include <esp_http_server.h>
#include <esp_log.h>
#include <unistd.h>

#include "filesystem.h"
#include "http_server.h"

/***************************
***** CONSTANTS ************
***************************/

#define MAX_CLIENT_CONNECTIONS  5

#define HTTPD_201               "201 Created"
#define HTTPD_507               "507 Insufficient Storage"
#define WEB_FILE_DEFAULT        "/index.html"
#define WEB_BUFFER_SIZE         4096

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
static void web_con(httpd_req_t *req);
static esp_err_t file_get_handler(httpd_req_t *req);
static esp_err_t file_put_handler(httpd_req_t *req);
static esp_err_t file_delete_handler(httpd_req_t *req);
static esp_err_t websocket_handler(httpd_req_t *req);
static void free_ws_msg(void *ptr);

/***************************
***** LOCAL VARIABLES ******
***************************/

static httpd_handle_t       server;
static msg_type_t           msg_type_ws_recv;

static const httpd_uri_t    file_get = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = &file_get_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t    file_put = {
    .uri = "/*",
    .method = HTTP_PUT,
    .handler = &file_put_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t    file_delete = {
    .uri = "/*",
    .method = HTTP_DELETE,
    .handler = &file_delete_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t    websocket = {
    .uri = "/websocket",
    .method = HTTP_GET,
    .handler = &websocket_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = false
};

static char web_buffer[WEB_BUFFER_SIZE];

/***************************
***** PUBLIC FUNCTIONS *****
***************************/

void http_init(void) {
    assert(!server);
    assert(!msg_type_ws_recv);
    msg_type_ws_recv = msg_register();
}

msg_type_t http_msg_type_ws_recv(void) {
    assert(msg_type_ws_recv);
    return msg_type_ws_recv;
}

void http_start(con_mode_t mode) {
    assert(msg_type_ws_recv);
    assert(!server);

    con_mode_t *mode_ptr = malloc(sizeof(con_mode_t));
    *mode_ptr = mode;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = MAX_CLIENT_CONNECTIONS;
    config.close_fn = &close_fn;
    config.global_user_ctx = mode_ptr;
    config.uri_match_fn = &httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &websocket);
        httpd_register_uri_handler(server, &file_get);
        httpd_register_uri_handler(server, &file_put);
        httpd_register_uri_handler(server, &file_delete);
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

void http_close(int sockfd) {
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
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        httpd_sess_trigger_close(server, client_fds[i]);
    }
}

void http_send_ws_msg(con_id_t con, const char *text) {
    assert(server);
    assert(text);

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

static void web_con(httpd_req_t *req) {
    int sockfd = httpd_req_to_sockfd(req);
    con_id_t con;
    if (con_get_con(sockfd, &con)) {
        con_ping(con);
    } else {
        con_mode_t mode = *(con_mode_t*)httpd_get_global_user_ctx(req->handle);
        con_create(mode, sockfd);
    }
}

static esp_err_t file_get_handler(httpd_req_t *req) {
    web_con(req);
    LOGI("GET %s", req->uri);
    const char *uri = req->uri;
    if (!strcmp(uri, "/")) {
        uri = WEB_FILE_DEFAULT;
    }
    char *content_type;
    int fd = fs_web_open(uri, FS_WEB_READ, &content_type);
    if (fd < 0) {
        httpd_resp_set_status(req, HTTPD_404);
        httpd_resp_send(req, NULL, 0);
    } else {
        httpd_resp_set_type(req, content_type);
        int16_t read;
        do {
            read = fs_web_read(fd, web_buffer, WEB_BUFFER_SIZE);
            if (read > 0) {
                httpd_resp_send_chunk(req, web_buffer, read);
            }
        } while (read == WEB_BUFFER_SIZE);
        httpd_resp_send_chunk(req, NULL, 0);
        fs_web_close(fd);
    }
    return ESP_OK;
}

static esp_err_t file_put_handler(httpd_req_t *req) {
    web_con(req);
    LOGI("PUT %s", req->uri);
    bool exist = fs_web_exist(req->uri);
    bool error = false;
    int fd = fs_web_open(req->uri, FS_WEB_WRITE, NULL);
    if (fd < 0) {
        httpd_resp_set_status(req, HTTPD_404);
    } else {
        size_t len = req->content_len;
        while ((len > 0) && !error) {
            int16_t chunk = WEB_BUFFER_SIZE;
            if (len < chunk) {
                chunk = len;
            }
            if (httpd_req_recv(req, web_buffer, chunk) == chunk) {
                if (fs_web_write(fd, web_buffer, chunk) != chunk) {
                    httpd_resp_set_status(req, HTTPD_507);
                    error = true;
                }
            } else {
                httpd_resp_set_status(req, HTTPD_500);
                error = true;
            }
            len -= chunk;
        }
        fs_web_close(fd);
        if (error) {
            fs_web_delete(req->uri);
        } else if (exist) {
            httpd_resp_set_status(req, HTTPD_204);
        } else {
            httpd_resp_set_status(req, HTTPD_201);
        }
    }
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t file_delete_handler(httpd_req_t *req) {
    web_con(req);
    LOGI("DELETE %s", req->uri);
    if (fs_web_exist(req->uri)) {
        fs_web_delete(req->uri);
        httpd_resp_set_status(req, HTTPD_204);
    } else {
        httpd_resp_set_status(req, HTTPD_404);
    }
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
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
    // TODO handle ping
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
                con_ping(con);
                ws_msg_t *ws_msg = calloc(1, sizeof(ws_msg_t));
                ws_msg->con = con;
                ws_msg->text = (char*)buf;
                msg_send_ptr(msg_type_ws_recv, ws_msg, &free_ws_msg);
            }
        }
    }
    return ret;
}

static void free_ws_msg(void *ptr) {
    ws_msg_t *ws_msg = ptr;
    free(ws_msg->text);
}
