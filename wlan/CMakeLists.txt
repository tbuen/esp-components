idf_component_register(SRCS "wlan.c"
                       INCLUDE_DIRS "include"
                       REQUIRES message
                       PRIV_REQUIRES cjson http_server filesystem log esp_wifi mdns)
