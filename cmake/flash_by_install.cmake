# 定义自定义构建目标：下载到目标单片机
set(OPENOCD_EXECUTABLE "openocd")

message(STATUS "Building SLAVE")
install(
  CODE CODE
  "MESSAGE(\"Flash SLAVE......\")"
  CODE "execute_process(COMMAND openocd -f
${PROJECT_SOURCE_DIR}/Scripts/OpenOCD/openocd_gdlink.cfg -c \"init; reset
halt; program ${PROJECT_SOURCE_DIR}/build/debug/GD32F4xx_Bootloader.elf reset\" -c
shutdown)")
