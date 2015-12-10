cmake_minimum_required(VERSION 3.3.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_BUILD_TYPE_INIT "Release")

project(Lunatic)

include_directories(${LUA_INCLUDE_DIR})
include_directories(${LUA_INCLUDE_EXTRA})
include_directories(${PYTHON_INCLUDE_DIR})

link_directories(${LUA_LIBRARIES})

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

add_library(python MODULE ${SOURCES})
add_library(lua MODULE ${SOURCES})

if (CGILUA_ENV)
    find_package(cgilua REQUIRED)
    include_directories(${cgilua_DIR})
    add_definitions(-DCGILUA_ENV=ON)
    target_link_libraries(lua ${cgilua_DIR}/Release/libcgilua.a)
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
    target_link_libraries(python pthread lua32ng lualib32ng ${PYTHON_LIBRARIES})
endif (UNIX)

target_link_libraries(lua pthread ${PYTHON_LIBRARIES} lua32ng lualib32ng)
