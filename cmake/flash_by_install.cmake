# 定义自定义构建目标：下载到目标单片机
set(OPENOCD_EXECUTABLE "openocd")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  install(
    CODE CODE
    "MESSAGE(\"Flash Debug......\")"
    CODE "execute_process(COMMAND openocd -f
    ${PROJECT_SOURCE_DIR}/Scripts/OpenOCD/openocd_gdlink.cfg -c \"init; reset
    halt; program ${PROJECT_SOURCE_DIR}/build/Debug/GD32F4xx_Bootloader.elf reset\" -c
    shutdown)")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  install(
    CODE CODE
    "MESSAGE(\"Flash Release......\")"
    CODE "execute_process(COMMAND openocd -f
    ${PROJECT_SOURCE_DIR}/Scripts/OpenOCD/openocd_gdlink.cfg -c \"init; reset
    halt; program ${PROJECT_SOURCE_DIR}/build/Release/GD32F4xx_Bootloader.elf reset\" -c
    shutdown)")
endif()
