# DTP over QUIC 
#
# Build and run by calling: 
# cmake -S. -Bbuild && cmake --build build
cmake_minimum_required(VERSION 3.13)

# ---- Project ----
project(DTPoverQuic
        VERSION 1.0.0.1
        DESCRIPTION "dtp layer over quiche"
        )

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib) #! this statement must be after the project description
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---- Fetch CPM ----

set(CPM_DOWNLOAD_VERSION 0.36.0) 
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
  message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
  file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()
include(${CPM_DOWNLOAD_LOCATION})

# ---- using FetchContent
# include(FetchContent)

# FetchContent_Declare(
#     Corrosion
#     GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
#     GIT_TAG v0.3.2 # Optionally specify a commit hash, version tag or branch here
# )
# # Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
# FetchContent_MakeAvailable(Corrosion)

CPMAddPackage("gh:corrosion-rs/corrosion@0.3.2")

# ---- Corrosion (Cmake-cargo)
corrosion_import_crate(
    MANIFEST_PATH quiche/quiche/Cargo.toml
    CRATES quiche
    FEATURES ffi
)

#quiche
execute_process(COMMAND git submodule init
                WORKING_DIRECTORY .)
 

# * You may use these statements to check the project path
# message (">>> top PROJECT_NAME: ${PROJECT_NAME}")
# message (">>> top PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
# message (">>> top <PROJECT_NAME>_SOURCE_DIR: ${${TOP_PROJECT_NAME}_SOURCE_DIR}") 
# message (">>> top PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")
# message (">>> top <PROJECT_NAME>_BINARY_DIR: ${${TOP_PROJECT_NAME}_BINARY_DIR}")
# message (">>> top CMAKE_PROJECT_NAME: ${CMAKE_PROJECT_NAME}")

include_directories(include)

#set(quiche-dir deps/quiche )

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

#find_package(OpenSSL REQUIRED)
#message(STATUS "root: ${OPENSSL_ROOT_DIR}")
#message(STATUS "OpenSSL_VERSION: ${OPENSSL_VERSION}")
#message(STATUS "OpenSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
#message(STATUS "OpenSSL_LIBRARIES: ${OPENSSL_LIBRARIES}")
#option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# dtplayer library
aux_source_directory(lib LIB_SRC)
add_library(dtplayer-core
    ${LIB_SRC}
)
#?
target_link_libraries(dtplayer-core 
    PUBLIC 
        quiche
)
target_include_directories(dtplayer-core PUBLIC include)
set_target_properties(dtplayer-core
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES)
target_compile_options(dtplayer-core PUBLIC
    $<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
    $<$<C_COMPILER_ID:MSVC>: >)

SET_TARGET_PROPERTIES(dtplayer-core PROPERTIES VERSION 1.2 SOVERSION 1)

# ---- example server
add_executable(server examples/server.c)
target_include_directories(server
    PUBLIC
        include
    PUBLIC
        examples
    PUBLIC
        lib
)

target_link_directories(server
    PUBLIC
        ${PROJECT_BINARY_DIR}
)

target_link_libraries(server
    PUBLIC
        ev # libev
        quiche
        dtplayer-core
        Threads::Threads
)
set_target_properties(server
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES)
target_compile_options(server PRIVATE
    $<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
    $<$<C_COMPILER_ID:MSVC>: >)
# ---- example client
add_executable(client examples/client.c)
target_include_directories(client
    PUBLIC
        include
    PUBLIC
        examples
    PUBLIC
        lib
)
    
target_link_directories(client
    PUBLIC
        ${PROJECT_BINARY_DIR}
)
    
target_link_libraries(client
    PUBLIC
        ev # libev
        quiche
        dtplayer-core
        Threads::Threads
)
set_target_properties(client
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES)
target_compile_options(client PRIVATE
    $<$<OR:$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:GNU>>: -Wpedantic -Wextra -Wall>
    $<$<C_COMPILER_ID:MSVC>: >)

get_property(dirs DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message(">>> include_dirs=${dirs}") #打印一下目录情况

#
# Adds clangformat as target that formats all source files
#
# get all project files for formatting
file(GLOB_RECURSE CLANG_FORMAT_SOURCE_FILES *.c *.h)
 