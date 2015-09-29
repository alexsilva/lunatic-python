//
// Created by alex on 9/29/15.
//

#ifndef LUNATIC_LSHARED_H
#define LUNATIC_LSHARED_H

#include <lua.h>

#define lapi_address(L, lo) ((lo)+L->stack.stack-1)

#endif //LUNATIC_LSHARED_H
