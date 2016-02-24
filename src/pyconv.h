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
    UNTOUCHED = 0,
    CONVERTED = 1,
    WRAP = 2
} Conversion;

void set_object_by_reference(lua_State *L, int n);
void set_unicode_string(lua_State *L, char *name, char *value);

char *get_unicode_encoding(lua_State *L);
char *get_unicode_errorhandler(lua_State *L);

py_object *py_object_container(lua_State *L, PyObject *obj, int asindx);
Conversion push_pyobject_container(lua_State *L, PyObject *obj, int asindx);
Conversion py_convert(lua_State *L, PyObject *o);
void lua_raw(lua_State *L);

void pyobject_as_string(lua_State *L, PyObject *o, String *str);
void pyobject_as_encoded_string(lua_State *L, PyObject *o, String *str);

#endif //LUNATIC_PYCONV_H
