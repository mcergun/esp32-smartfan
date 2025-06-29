idf_component_register(SRCS "fan_controller.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver freertos dht)