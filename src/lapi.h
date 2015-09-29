//
// Created by alex on 25/09/2015.
// Missing functions inside shared lib

#ifndef LUNATIC_LAPI_H
#define LUNATIC_LAPI_H

#include <lua.h>
#include "lshared.h"

int lapi_next(lua_State *L, lua_Object o, int i);

#define lua_next(state, obj, index) lapi_next(state, obj, index);

#endif //LUNATIC_LAPI_H
