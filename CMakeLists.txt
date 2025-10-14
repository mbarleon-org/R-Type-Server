cmake_minimum_required(VERSION 3.16)
project(r-type_server LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

if (UNIX AND NOT APPLE)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_SKIP_INSTALL_RPATH FALSE)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_RPATH "\$ORIGIN")
    set(CMAKE_INSTALL_RPATH "\$ORIGIN")
elseif (APPLE)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_SKIP_INSTALL_RPATH FALSE)
    set(CMAKE_MACOSX_RPATH TRUE)
    set(CMAKE_BUILD_RPATH "@loader_path")
    set(CMAKE_INSTALL_RPATH "@loader_path")
endif ()

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif ()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

include(ProcessorCount)
ProcessorCount(N)
if (NOT N EQUAL 0)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${N})
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB_RECURSE SRC_R_TYPE_SERVER "core/src/*.cpp")

add_executable(r-type_server ${SRC_R_TYPE_SERVER})
target_include_directories(r-type_server PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/server/include
        ${CMAKE_SOURCE_DIR}/network/include
)

add_subdirectory(ecs)
target_link_libraries(r-type_server PRIVATE r-engine)

add_subdirectory(server)
target_link_libraries(r-type_server PRIVATE r-type_srv)

add_subdirectory(network)
target_link_libraries(r-type_server PRIVATE r-type_network)

function(enable_strict_warnings tgt)
    if (MSVC)
        target_compile_options(${tgt} PRIVATE
                /W4
                /WX
                /permissive-
                /EHsc
                /Zc:__cplusplus
        )
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        target_compile_options(${tgt} PRIVATE
                -Wall -Wextra -Werror -Wpedantic
                -Wconversion -Wsign-conversion
                -Wshadow -Wnull-dereference
                -Wundef -Wuninitialized
                -Wcast-align -Wcast-qual
                -Wswitch-default
                -Wdouble-promotion
                -Wformat=2
                -Wwrite-strings
        )
    else ()
        target_compile_options(${tgt} PRIVATE
                -Wall -Wextra -Werror -Wpedantic
                -Wconversion -Wsign-conversion
                -Wshadow -Wnull-dereference
                -Wundef -Wuninitialized -Winit-self
                -Wredundant-decls
                -Wcast-align -Wcast-qual
                -Wmissing-declarations -Wswitch-default
                -Wdouble-promotion -Wformat=2 -Wwrite-strings
        )
    endif ()
endfunction()

enable_strict_warnings(r-type_server)

option(ENABLE_DEBUG "Enable debug macros and flags" OFF)
if (ENABLE_DEBUG)
    target_compile_definitions(r-type_server PRIVATE DEBUG=1)
endif ()

if (MSVC)
    set(_rm_script "${CMAKE_CURRENT_BINARY_DIR}/rm_msvc_extras.cmake")
    file(WRITE "${_rm_script}" [=[
        if(DEFINED dir)
            file(GLOB _files "${dir}/*.lib" "${dir}/*.exp")
            if(_files)
                file(REMOVE ${_files})
            endif()
        endif()
    ]=])
    add_custom_command(TARGET r-type_server POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            -Ddir="$<TARGET_FILE_DIR:r-type_server>"
            -P "${_rm_script}"
            COMMENT "Removing MSVC import library and export files"
    )
endif ()
