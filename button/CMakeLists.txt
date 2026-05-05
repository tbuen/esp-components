idf_component_register(SRCS "button.c"
                       INCLUDE_DIRS "include"
                       REQUIRES message
                       PRIV_REQUIRES log esp_driver_gpio)
