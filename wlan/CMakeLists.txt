idf_component_register(SRCS "wlan.c"
                       INCLUDE_DIRS "include"
                       REQUIRES message
                       PRIV_REQUIRES http_server log esp_wifi mdns)
