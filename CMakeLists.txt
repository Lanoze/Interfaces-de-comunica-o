

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(DisplayC C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(DisplayC DisplayC.c inc/ssd1306.c)

pico_set_program_name(DisplayC "DisplayC")
pico_set_program_version(DisplayC "0.1")

#Parece que estava dando problema com a palavra "PRIVATE"
# Add the standard library to the build
target_link_libraries(DisplayC
        pico_stdlib
        hardware_pio
        )

# Add the standard include files to the build
target_include_directories(DisplayC PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(DisplayC 
        hardware_i2c
        )

       file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)
       pico_generate_pio_header(DisplayC ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)
        pico_enable_stdio_uart(DisplayC 1)
        pico_enable_stdio_usb(DisplayC 1)
        target_sources(DisplayC PRIVATE DisplayC.c)
#target_link_libraries(DisplayC PRIVATE pico_stdlib hardware_pio) "PRIVATE" era o problema
        pico_add_extra_outputs(DisplayC) 

