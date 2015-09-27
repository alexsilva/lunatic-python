//
// Created by alex on 26/09/2015.
//

#ifndef LUNATIC_LUACONV_H
#define LUNATIC_LUACONV_H

#include "stdbool.h"

#define POBJECT "POBJECT"
#define ASINDX "ASINDX"

typedef struct {
    PyObject *o;
    int asindx;
} py_object;

int lua_isboolean(lua_State *L, lua_Object obj);
int lua_getboolean(lua_State *L, lua_Object obj);

int lua_gettop_c(lua_State *L);
int get_base_tag(lua_State *L);

PyObject *get_py_tuple(lua_State *L, lua_Object ltable, bool stacked, bool wrapped);
PyObject *get_py_dict(lua_State *L, lua_Object ltable);

void py_kwargs(lua_State *L);
void py_args(lua_State *L);

PyObject *lua_convert(lua_State *L, int n);

#endif //LUNATIC_LUACONV_H
