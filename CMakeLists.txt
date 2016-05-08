cmake_minimum_required(VERSION 3.3.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")

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
    src/constants.h
    src/lshared.c)

if (WIN32)
    set(SOURCES ${SOURCES} src/lapi.c)
endif()

add_library(python MODULE ${SOURCES})
add_library(lua MODULE ${SOURCES})

if (CGILUA_ENV)
    find_package(cgilua REQUIRED)
    include_directories(${cgilua_DIR})
    add_definitions(-DCGILUA_ENV=ON)
    set(CGILUA_LIBS ${cgilua_DIR}/Release/libcgilua.a)
    if (UNIX) # LINUX
        find_package(lua REQUIRED)
        link_directories(lua_DIR)
        set(CGILUA_LIBS ${CGILUA_LIBS} ${lua_DIR}/Release/liblua.a)
    endif()
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
    set(LINK_LIBRARY python2.7 pthread util)
    target_link_libraries(python ${LINK_LIBRARY})
    target_link_libraries(lua ${LINK_LIBRARY})
else(UNIX)
    set(LINK_LIBRARY python27 lua32ng lualib32ng)
    target_link_libraries(python ${LINK_LIBRARY})
    target_link_libraries(lua ${LINK_LIBRARY})
endif (UNIX)


