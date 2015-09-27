cmake_minimum_required(VERSION 3.3.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_BUILD_TYPE_INIT "Release")

project(Lunatic)

include_directories(${LUA_INCLUDE_DIR})
include_directories(${LUA_INCLUDE_EXTRA})
include_directories(${PYTHON_INCLUDE_DIR})

if (UNIX)
    link_directories(${PYTHON_LIBRARIES})
endif (UNIX)

set(PYTHON_SOURCES
    src/pythoninlua.h
    src/pythoninlua.c
    src/lapi.c
    src/lapi.h)

add_library(python MODULE ${PYTHON_SOURCES})

add_library(lua MODULE
        ${PYTHON_SOURCES}
        src/luainpython.h
        src/luainpython.c)

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set_target_properties(lua PROPERTIES
            PREFIX ""
            SUFFIX ".pyd")
else ()
    set_target_properties(lua PROPERTIES
            PREFIX "")
endif ()

# =====================
set_target_properties(python PROPERTIES
        COMPILE_FLAGS "-m32"
        LINK_FLAGS    "-m32")

set_target_properties(lua PROPERTIES
        COMPILE_FLAGS "-m32"
        LINK_FLAGS    "-m32")
# =====================
if (UNIX)
    target_link_libraries(python python2.7 pthread util ${LUA_LIBRARIES})
else()
    target_link_libraries(python ${PYTHON_LIBRARIES} ${LUA_LIBRARIES})
endif (UNIX)

target_link_libraries(lua ${PYTHON_LIBRARIES} ${LUA_DLL} ${LUA_LIBRARY})