//
// Created by alex on 9/29/15.
//

#ifndef LUNATIC_LSHARED_H
#define LUNATIC_LSHARED_H

#include <lua.h>
#include "ltable.h"

int lraw_next(lua_State *L, lua_Object lobj, int index, Node **n);

#define lapi_address(L, lo) ((lo)+L->stack.stack-1)

#define lua_getkey(L, n) (ref(L, n))
#define lua_getstr(o) (svalue(o))
#define lua_getnum(o) (nvalue(o))
#define lua_gettype(o) (ttype(o))

#endif //LUNATIC_LSHARED_H
