idf_component_register(SRCS "http_server.c"
                       INCLUDE_DIRS "include"
                       REQUIRES message connection
                       PRIV_REQUIRES log esp_http_server)
