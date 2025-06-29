idf_component_register(SRCS "ble_controller.c"
                       INCLUDE_DIRS "."
                       REQUIRES bt fan_controller) # Add any private requirements for your component
                       # In this case, esp_idf_lib_helpers is often needed for common utilities
                       # that might be implicitly used by ESP-IDF components.
                       # You might also need "freertos" or "driver" etc.
                       # depending on what specific functions are called directly.