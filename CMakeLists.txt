cmake_minimum_required(VERSION 2.8.8)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

project(Lunatic)

include_directories(${LUA_INCLUDE_DIR})
include_directories(${LUA_INCLUDE_EXTRA})
include_directories(${PYTHON_INCLUDE_DIR})

link_directories(${PYTHON_LIBRARIES})

add_library(python MODULE
        src/pythoninlua.h
        src/pythoninlua.c)
# =====================
set_target_properties(python PROPERTIES
        COMPILE_FLAGS "-m32"
        LINK_FLAGS    "-m32")
# =====================

target_link_libraries(python python2.7 pthread util ${LUA_LIBRARIES})

