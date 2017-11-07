//
// Created by alex on 07/11/2017.
//

#ifndef LUA_LEXCEPTION_H
#define LUA_LEXCEPTION_H
#include <Python.h>
#include "lua.h"
void lua_error_fallback(lua_State *L, PyObject *exc, const char *msg);
#endif //LUA_LEXCEPTION_H
