//
// Created by alex on 26/09/2015.
//

#ifndef LUNATIC_PYCONV_H
#define LUNATIC_PYCONV_H

#include "Python.h"
#include "lua.h"

#define LuaObject_Check(op) PyObject_TypeCheck(op, &LuaObject_Type)

lua_State *LuaState;
PyTypeObject LuaObject_Type;

PyObject *LuaObject_New(int n);

typedef struct {
    PyObject_HEAD
    int ref;
    int refiter;
} LuaObject;

int py_object_wrap_lua(lua_State *L, PyObject *pobj, int asindx);
int py_convert(lua_State *L, PyObject *o);

#endif //LUNATIC_PYCONV_H
