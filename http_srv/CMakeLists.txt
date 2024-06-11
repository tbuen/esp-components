idf_component_register(SRCS "http_srv.c"
                       INCLUDE_DIRS "include"
                       REQUIRES connection
                       PRIV_REQUIRES message esp_http_server)
