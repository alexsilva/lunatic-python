//
// Created by alex on 26/09/2015.
//

#ifndef LUNATIC_LUACONV_H
#define LUNATIC_LUACONV_H

#include "stdbool.h"
#include "luainpython.h"

#define POBJECT "POBJECT"
#define ASINDX "ASINDX"

typedef struct {
    PyObject *o;
    int asindx;
} py_object;

int lua_isboolean(lua_State *L, lua_Object obj);
int lua_getboolean(lua_State *L, lua_Object obj);

int lua_gettop(lua_State *L);
py_object *get_py_object(lua_State *L, int n);
int is_wrapped_object(lua_State *L, lua_Object lobj);
int is_indexed_array(lua_State *L, lua_Object lobj);
int get_base_tag(lua_State *L);

PyObject *get_py_tuple(lua_State *, int stackpos);
PyObject *_get_py_tuple(lua_State *, lua_Object lobj);

PyObject *get_py_dict(lua_State *L, lua_Object ltable);

void py_kwargs(lua_State *L);
void py_args(lua_State *L);

PyObject *lua_convert(lua_State *L, int stackpos);
PyObject *lua_stack_convert(lua_State *L, int stackpos, lua_Object lobj);
PyObject *lua_interpreter_object_convert(InterpreterObject *interpreterObject,
                                         int stackpos, lua_Object lobj);
PyObject *lua_interpreter_stack_convert(InterpreterObject *interpreterObject,
                                        int stackpos);
#endif //LUNATIC_LUACONV_H
