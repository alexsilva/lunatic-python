cmake_minimum_required(VERSION 3.3.0)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

project(Lunatic)

include_directories(
    ${LUA_INCLUDE_DIR}
    ${LUA_INCLUDE_EXTRA}
    ${PYTHON_INCLUDE_DIR}
)

link_directories(${LUA_LIBRARIES} ${PYTHON_LIBRARIES})

set(SOURCES
    deps/vstring/vstring.h
    deps/strdup/strdup.h
    src/luainpython.c
    src/pythoninlua.c
    src/luaconv.c
    src/pyconv.c
    src/utils.h
    src/utils.c
    src/lshared.h
    src/constants.h
    src/lshared.c
    src/auxiliary.c
    src/auxiliary.h
    src/stack.c
    src/stack.h
    src/lexception.c
    src/lexception.h)

if (WIN32)
    set(SOURCES ${SOURCES} src/lapi.c)
endif()

add_library(python MODULE ${SOURCES})
add_library(lua MODULE ${SOURCES})

set(LUA_LINK_LIBS lua32ngw lualib32ngw)

if (CGILUA_ENV)
    add_definitions(-DCGILUA_ENV=ON)
    include_directories(${cgilua_SOURCE_DIR})
    set(LUA_LINK_LIBS cgilua)
endif()

target_compile_definitions(python PRIVATE python_COMPAT)

if (WIN32)
    set_target_properties(lua PROPERTIES PREFIX "" SUFFIX ".pyd")
    set(LINK_LIBRARY python37)
else()
    set_target_properties(lua PROPERTIES PREFIX "")
    set(LINK_LIBRARY python3.7m)
endif()


target_link_libraries(python ${LINK_LIBRARY} ${LUA_LINK_LIBS})
target_link_libraries(lua ${LINK_LIBRARY} ${LUA_LINK_LIBS})