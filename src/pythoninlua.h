/*

 Lunatic Python
 --------------
 
 Copyright (c) 2002-2005  Gustavo Niemeyer <gustavo@niemeyer.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
#ifndef PYTHONINLUA_H
#define PYTHONINLUA_H

#include "lua.h"

#define POBJECT "POBJECT"
#define ASINDX "ASINDX"

#if defined(WIN32) //  Microsoft
#define LUA_API __declspec(dllexport)
#else //  GCC
#define LUA_API __attribute__((visibility("default")))
#endif

PyTypeObject LuaObject_Type;

#define LuaObject_Check(op) PyObject_TypeCheck(op, &LuaObject_Type)

typedef struct {
    PyObject_HEAD
    int ref;
    int refiter;
} LuaObject;

typedef struct {
    PyObject *o;
    int asindx;
} py_object;

py_object* luaPy_to_pobject(lua_State *L, int n);
LUA_API int luaopen_python(lua_State *L);

#endif
