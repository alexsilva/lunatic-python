//
// Created by alex on 26/09/2015.
//

#ifndef LUNATIC_LUACONV_H
#define LUNATIC_LUACONV_H

#include "stdbool.h"
#include "luainpython.h"

typedef struct {
    PyObject *object;
    bool asindx;
    bool isargs;
    bool iskwargs;
} py_object;

int lua_gettop(lua_State *L);
PyObject *get_pobject(lua_State *L, lua_Object userdata);
py_object *get_py_object(lua_State *L, lua_Object userdata);
int is_object_container(lua_State *L, lua_Object lobj);
bool is_indexed_array(lua_State *L, lua_Object ltable);
bool ispyargs(lua_State *L, lua_Object userdata);
bool ispykwargs(lua_State *L, lua_Object userdata);

PyObject *get_py_tuple(lua_State *, int stackpos);
PyObject *ltable2tuple(lua_State *, lua_Object lobj);
PyObject *ltable2list(lua_State *L, lua_Object ltable);
PyObject *get_py_dict(lua_State *L, lua_Object ltable);

void py_kwargs(lua_State *L);
void py_args(lua_State *L);

PyObject *lua_object_convert(lua_State *L, lua_Object lobj);
PyObject *lua_stack_convert(lua_State *L, int stackpos);
PyObject *lua_interpreter_object_convert(InterpreterObject *interpreter,
                                         lua_Object lobj);
PyObject *lua_interpreter_stack_convert(InterpreterObject *interpreter,
                                        int stackpos);
#endif //LUNATIC_LUACONV_H
