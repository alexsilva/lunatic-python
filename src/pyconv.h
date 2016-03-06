//
// Created by alex on 26/09/2015.
//

#ifndef LUNATIC_PYCONV_H
#define LUNATIC_PYCONV_H

#include "Python.h"
#include "lua.h"
#include "luainpython.h"
#include "luaconv.h"

#define LuaObject_Check(op) PyObject_TypeCheck(op, &LuaObject_Type)

typedef struct {
    PyObject_HEAD
    InterpreterObject *interpreter;
    int ref;
    int refiter;
    bool indexed;
} LuaObject;

typedef struct STRING {
    char *buff;
    int size;
} String;

typedef enum {
    UNCHANGED = 0,
    CONVERTED = 1,
    WRAPPED = 2
} Conversion;

py_object *py_object_container(lua_State *L, PyObject *obj, bool asindx);
Conversion push_pyobject_container(lua_State *L, PyObject *obj, bool asindx);
Conversion py_convert(lua_State *L, PyObject *o);
void py_raw(lua_State *L);

void get_pyobject_string_buffer(lua_State *L, PyObject *obj, String *str);
PyObject *get_pyobject_encoded_string_buffer(lua_State *L, PyObject *obj, String *str);

#endif //LUNATIC_PYCONV_H
