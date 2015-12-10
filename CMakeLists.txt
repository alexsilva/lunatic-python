cmake_minimum_required(VERSION 3.3.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_BUILD_TYPE_INIT "Release")

project(Lunatic)

include_directories(${LUA_INCLUDE_DIR})
include_directories(${LUA_INCLUDE_EXTRA})
include_directories(${PYTHON_INCLUDE_DIR})

link_directories(${LUA_LIBRARIES})
link_directories(${PYTHON_LIBRARIES})

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
    set(CGILUA_LIBS ${cgilua_DIR}/Release/libcgilua.a)
    target_link_libraries(lua ${CGILUA_LIBS})
    target_link_libraries(python ${CGILUA_LIBS})
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
    target_link_libraries(python python2.7 pthread util)
    target_link_libraries(lua pthread ${PYTHON_LIBRARIES})
else(UNIX)
    target_link_libraries(python pthread python27 lua32ng lualib32ng)
    target_link_libraries(lua pthread python27 lua32ng lualib32ng)
endif (UNIX)


