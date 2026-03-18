cmake_minimum_required(VERSION 3.22)

# 1. Cross-compiler setup (Telling CMake we are building for ARM, not PC)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(DroneFirmware_H723 C ASM)

# 2. Find source files (.c and .s)
file(GLOB_RECURSE SOURCES "Src/*.c")
set(ASM_SOURCES "${CMAKE_SOURCE_DIR}/Startup/startup_stm32h723zgtx.s")

# 3. Create the executable target
add_executable(${PROJECT_NAME}.elf ${SOURCES} ${ASM_SOURCES})

# 4. Microcontroller definition (Equivalent to #define STM32H723xx in C)
target_compile_definitions(${PROJECT_NAME}.elf PRIVATE STM32H723xx)

# 5. Compiler & Linker Flags (Hardware specifics and optimization)
set(MCU_FLAGS "-mcpu=cortex-m7" "-mthumb" "-mfpu=fpv5-d16" "-mfloat-abi=hard")

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS} -O2 -Wall -Wextra -ffunction-sections -fdata-sections
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${CMAKE_SOURCE_DIR}/STM32H723ZGTX_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nano.specs --specs=nosys.specs
)

# 6. Include directories (Where to look for .h files)
file(GLOB_RECURSE CMSIS_HEADERS "Src/CMSIS/*.h")
foreach(HEADER ${CMSIS_HEADERS})
    get_filename_component(DIR ${HEADER} DIRECTORY)
    list(APPEND CMSIS_DIRS ${DIR})
endforeach()
if(CMSIS_DIRS)
    list(REMOVE_DUPLICATES CMSIS_DIRS)
endif()

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    Src/Config Src/Init Src/Drivers Src/Devices Src/Utils ${CMSIS_DIRS}
)

# 7. Post-build actions (Generate .bin, .hex and print size)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
)