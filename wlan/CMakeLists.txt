idf_component_register(SRCS "wlan.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES message button http_srv log esp_wifi mdns)
