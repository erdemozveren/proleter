cmake_minimum_required(VERSION 3.16)

project(proleter_vm C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(proleter-vm
    vm/main.c
    vm/vm.c
    vm/vm_decoding.c
)

target_include_directories(proleter-vm PRIVATE
    vm/include
)

target_compile_options(proleter-vm PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(proleter-vm PRIVATE
        -Wconversion
    )
endif()

file(GLOB RUNTIME_LIBS CONFIGURE_DEPENDS
    vm/runtime-libs/*.c
)

foreach(runtime_src ${RUNTIME_LIBS})
    get_filename_component(runtime_name ${runtime_src} NAME_WE)

    add_library(${runtime_name} SHARED
        ${runtime_src}
    )

    target_include_directories(${runtime_name} PRIVATE
        vm/include
    )

    set_target_properties(${runtime_name} PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/modules
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/modules
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/modules
    )
endforeach()

add_custom_command(TARGET proleter-vm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/modules
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_BINARY_DIR}/modules
            ${CMAKE_SOURCE_DIR}/modules
    )

