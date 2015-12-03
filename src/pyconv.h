//
// Created by alex on 26/09/2015.
//

#ifndef LUNATIC_PYCONV_H
#define LUNATIC_PYCONV_H

#include "Python.h"
#include "lua.h"
#include "luainpython.h"

#define LuaObject_Check(op) PyObject_TypeCheck(op, &LuaObject_Type)

PyObject *LuaObject_PyNew(lua_State *L, lua_Object lobj);
PyObject *LuaObject_New(lua_State *L, int n);

typedef struct {
    PyObject_HEAD
    lua_State *L;
    int ref;
    int refiter;
} LuaObject;

int py_object_wrap_lua(lua_State *L, PyObject *pobj, int asindx);
int py_convert(lua_State *L, PyObject *o);
void lua_raw(lua_State *L);

#endif //LUNATIC_PYCONV_H
