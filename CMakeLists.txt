cmake_minimum_required(VERSION 3.8)

set(CMAKE_CXX_STANDARD 11)

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
    src/utils.c
    src/lshared.c
    src/auxiliary.c
    src/stack.c)

if (WIN32)
    set(SOURCES ${SOURCES} src/lapi.c)
endif()

SET_SOURCE_FILES_PROPERTIES( ${SOURCES} PROPERTIES LANGUAGE CXX)

add_library(python MODULE ${SOURCES})
add_library(lua MODULE ${SOURCES})

if (CGILUA_ENV)
    add_definitions(-DCGILUA_ENV=ON)
    include_directories(${cgilua_SOURCE_DIR})
    set(LIBRARIES cgilua)
    if (UNIX) # LINUX
        find_package(lua REQUIRED)
        link_directories(lua_DIR)
        set(LIBRARIES ${LIBRARIES} ${lua_DIR}/Release/liblua.a)
    endif()
    target_link_libraries(lua ${LIBRARIES})
    target_link_libraries(python ${LIBRARIES})
endif()

if (WIN32)
    set_target_properties(lua PROPERTIES PREFIX "" SUFFIX ".pyd")
else ()
    set_target_properties(lua PROPERTIES PREFIX "")
endif()

set_target_properties(python PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
set_target_properties(lua PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")

if (UNIX)
    set(LINK_LIBRARY python2.7)
    target_link_libraries(python ${LINK_LIBRARY})
    target_link_libraries(lua ${LINK_LIBRARY})
else()
    target_link_libraries(python python27 lua32ng lualib32ng)
    target_link_libraries(lua python27 lua32ngw lualib32ngw)
endif ()


