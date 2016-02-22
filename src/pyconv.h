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

typedef struct STRING_UNICODE {
    char *encoding;
    char *errors;
    bool strdup;
} StringUnicode;

extern StringUnicode *stringUnicode;

lua_Object py_object_wrapped(lua_State *L, PyObject *pobj, int asindx);
Conversion py_object_wrap_lua(lua_State *L, PyObject *pobj, int asindx);
Conversion py_convert(lua_State *L, PyObject *o);
void lua_raw(lua_State *L);

void pyobject_as_string(lua_State *L, PyObject *o, String *str);
void pyobject_as_encoded_string(lua_State *L, PyObject *o, String *str);

#endif //LUNATIC_PYCONV_H
