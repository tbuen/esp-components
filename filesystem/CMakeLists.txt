idf_component_register(SRCS "filesystem.c"
                       INCLUDE_DIRS "include"
                       REQUIRES cjson
                       PRIV_REQUIRES log fatfs mbedtls)
