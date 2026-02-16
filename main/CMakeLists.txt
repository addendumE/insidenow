file(GLOB SOURCES 
    "main.c"
    "dmx.c"
    "led.c"
    "ws2812.c"
    "now.c"
    "console.c"
    "nvs.c"
    "ws2812_bitbang.c"
)

idf_component_register(SRCS
SRCS ${SOURCES}
INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-maybe-uninitialized)
