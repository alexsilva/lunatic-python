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

extern "C"
{
#include <lua.h>
#include "stack.h"

// Extension version python
#define PY_EXT_VERSION (ptrchar "2.4.0")

#if defined(_WIN32) //  Microsoft
#define LUA_API __declspec(dllexport)
#else //  GCC
#define LUA_API __attribute__((visibility("default")))
#endif

LUA_API int luaopen_python(lua_State *L);
}

class PyUnicode {
public:
    const char *encoding = "UTF-8";
    const char *errorhandler =  "strict";;
};

class Lua {
public:
    Lua() = default;
    bool tconvert = false; /* table convert */
    bool embedded = false;
    int get_tag() { return this->tag; }
    void set_tag(int tag) { this->tag = tag; }
protected:
    int tag = -1; /* api tag */
};

class Python {
public:
    explicit Python(lua_State *L);
    PyUnicode unicode;
    bool object_ref;
    bool embedded;
    STACK stack;
    Lua lua;
};
#endif
