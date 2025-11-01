cmake_minimum_required(VERSION 3.16)
project(r-type_network LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

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

file(GLOB_RECURSE SRC_R_TYPE_SERVER "src/*.cpp")
add_library(r-type_srv SHARED ${SRC_R_TYPE_SERVER})
target_include_directories(r-type_srv PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ecs/include
)
target_link_libraries(r-type_srv PUBLIC
    r-type_network
    r-engine
)
target_compile_definitions(r-type_srv PRIVATE R_TYPE_SRV_BUILD=1)
target_compile_definitions(r-engine PUBLIC ECS_SERVER_MODE)

# find_package(Criterion REQUIRED)
# file(GLOB TEST_SRC tests/test_*.cpp)
# add_executable(rtype_srv_unit_tests ${TEST_SRC})
# target_link_libraries(rtype_srv_unit_tests PRIVATE r-type_srv Criterion::Criterion)
# target_include_directories(rtype_srv_unit_tests PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../network/include
# )

find_package(OpenSSL REQUIRED)
target_link_libraries(r-type_srv PRIVATE OpenSSL::SSL OpenSSL::Crypto)

function(enable_coverage_flags tgt)
    if(APPLE)
        target_compile_options(${tgt} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(${tgt} PRIVATE -fprofile-instr-generate -Wl,-u,___llvm_profile_runtime)
    endif()
endfunction()

option(ENABLE_TESTS "Enable test macros and flags" OFF)
if(ENABLE_TESTS)
    enable_coverage_flags(r-type_srv)
    # enable_coverage_flags(rtype_srv_unit_tests)

    enable_testing()
    add_test(NAME rtype_srv_unit_tests COMMAND rtype_srv_unit_tests)
endif()

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

enable_strict_warnings(r-type_srv)
# enable_strict_warnings(rtype_srv_unit_tests)

option(ENABLE_DEBUG "Enable debug macros and flags" OFF)
if (ENABLE_DEBUG)
    target_compile_definitions(r-type_srv PRIVATE DEBUG=1)
endif ()