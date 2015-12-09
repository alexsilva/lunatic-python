//
// Created by alex on 26/09/2015.
//

#ifndef LUNATIC_PYCONV_H
#define LUNATIC_PYCONV_H

#include "Python.h"
#include "lua.h"
#include "luainpython.h"

#define LuaObject_Check(op) PyObject_TypeCheck(op, &LuaObject_Type)

typedef struct {
    PyObject_HEAD
    InterpreterObject *interpreter;
    lua_State *L;
    int ref;
    int refiter;
} LuaObject;

typedef enum {
    UNTOUCHED = 0,
    CONVERTED = 1,
    WRAP = 2
} Conversion;

Conversion py_object_wrap_lua(lua_State *L, PyObject *pobj, int asindx);
Conversion py_convert(lua_State *L, PyObject *o);
void lua_raw(lua_State *L);

#endif //LUNATIC_PYCONV_H
