file(GLOB_RECURSE SOURCES "*.c")

idf_component_register(
    SRCS
    ${SOURCES}
    INCLUDE_DIRS
    "include"
    REQUIRES esp_driver_uart esp_driver_gpio esp_timer)
