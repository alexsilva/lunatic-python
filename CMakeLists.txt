cmake_minimum_required(VERSION 3.3.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_BUILD_TYPE_INIT "Release")

project(Lunatic)

include_directories(${LUA_INCLUDE_DIR})
include_directories(${LUA_INCLUDE_EXTRA})
include_directories(${PYTHON_INCLUDE_DIR})

link_directories(${LUA_LIBRARIES})

if (WIN32)
    find_package(pthreads-w32 REQUIRED)
    include_directories(${pthreads-w32_DIR})
    link_directories(${pthreads-w32_DIR}/Release)
endif(WIN32)

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
    set(SOURCES
        ${SOURCES}
        src/cgilua/cgilua.c
        src/loadlib/loadlib.c)
endif(CGILUA_ENV)

add_library(python MODULE ${SOURCES})
add_library(lua MODULE ${SOURCES})

if (CGILUA_ENV)
    target_compile_definitions(lua PRIVATE CGILUA_ENV=${CGILUA_ENV})
endif(CGILUA_ENV)

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
    target_link_libraries(python python2.7 pthread util pthreads)
else(UNIX)
    target_link_libraries(python pthreads lua32ng lualib32ng ${PYTHON_LIBRARIES})
endif (UNIX)

target_link_libraries(lua pthreads ${PYTHON_LIBRARIES} lua32ng lualib32ng)
