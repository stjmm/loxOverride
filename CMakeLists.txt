cmake_minimum_required(VERSION 3.15...4.0)

project(loxOverride
	VERSION 0.0.1
	DESCRIPTION "implementation of clox - with extra features"
	LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_EXTENSIONS OFF)

if(MSVC)
	add_compile_options(/W4 /permissive-)
else()
	add_compile_options(-Wall -Wextra -Wpedantic)
endif()

file(GLOB CLOX_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(clox ${CLOX_SOURCES})

target_compile_definitions(clox PRIVATE
    $<$<CONFIG:Debug>:DEBUG_BUILD>
    $<$<CONFIG:Release>:NDEBUG>
)
target_link_libraries(clox m)

if(NOT MSVC)
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()
