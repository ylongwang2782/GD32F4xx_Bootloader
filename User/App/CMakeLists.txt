target_sources(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/boot_main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ymodem.c
        ${CMAKE_CURRENT_SOURCE_DIR}/flash_if.c
        ${CMAKE_CURRENT_SOURCE_DIR}/common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/menu.c

)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
