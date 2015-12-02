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

set(SOURCES
    src/luainpython.c
    src/pythoninlua.c
    src/luaconv.c
    src/pyconv.c
    src/utils.h
    src/utils.c
    src/lshared.h
    src/lapi.c)

if (CGILUA_ENV)
    link_directories(src/cgilua src/loadlib)
    set(SOURCES
        src/cgilua/cgilua.c
        src/loadlib/loadlib.c)
endif(CGILUA_ENV)

add_library(python MODULE ${SOURCES})
add_library(lualib MODULE ${SOURCES})

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set_target_properties(lualib PROPERTIES
            PREFIX ""
            SUFFIX ".pyd")
else ()
    set_target_properties(lualib PROPERTIES
            PREFIX "")
endif ()

# =====================
set_target_properties(python PROPERTIES
        COMPILE_FLAGS "-m32"
        LINK_FLAGS    "-m32")

set_target_properties(lualib PROPERTIES
        COMPILE_FLAGS "-m32"
        LINK_FLAGS    "-m32")
# =====================
if (UNIX)
    target_link_libraries(python python2.7 pthread util ${LUA_LIBRARIES})
else()
    target_link_libraries(python ${PYTHON_LIBRARIES} ${LUA_DLL} ${LUA_LIBRARY})
endif (UNIX)

target_link_libraries(lualib ${PYTHON_LIBRARIES} ${LUA_DLL} ${LUA_LIBRARY})