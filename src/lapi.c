//
// Created by alex on 25/09/2015.
//
// Functions api that unfortunately were not found in the cgilua.

#include "lapi.h"
#include <ldo.h>
#include <lmem.h>

#ifndef STACK_LIMIT
#define STACK_LIMIT  INT_MAX  /* arbitrary limit */
#endif
#define STACK_UNIT    128

/*
** generic allocation routine.
*/
void *luaM_realloc(lua_State *L, void *block, unsigned long size) {
    size_t s = (size_t) size;
    if (s != size)
        lua_error(L, "memory allocation error: block too big");
    if (size == 0) {
        free(block);  /* block may be NULL, that is OK for free */
        return NULL;
    }
    block = realloc(block, s);
    if (block == NULL)
        lua_error(L, memEM);
    return block;
}

// Check stack size
void luaD_checkstack(lua_State *L, int n) {
    struct Stack *S = &L->stack;
    if (S->last - S->top <= n) {
        StkId top = S->top - S->stack;
        int stacksize = (S->last - S->stack) + STACK_UNIT + n;
        luaM_reallocvector(L, S->stack, stacksize, TObject);
        S->last = S->stack + (stacksize - 1);
        S->top = S->stack + top;
        if (stacksize >= STACK_LIMIT) {  /* stack overflow? */
            if (lua_stackedfunction(L, 100) == LUA_NOOBJECT)  /* 100 funcs on stack? */
                lua_error(L, "Lua2C - C2Lua overflow"); /* doesn't look like a rec. loop */
            else
                lua_error(L, "stack size overflow");
        }
    }
}

// Place a new object at the top of the stack.
static void lapi_pushobject(lua_State *L, TObject *o) {
    *L->stack.top = *o;
    incr_top;
}

static void lapi_top2L(lua_State *L, int n) {
    /* Put the 'n' elements on the top as the Lua2C contents */
    L->Cstack.base = (L->stack.top - L->stack.stack);  /* new base */
    L->Cstack.lua2C = L->Cstack.base - n;  /* position of the new results */
    L->Cstack.num = n;  /* number of results */
}

static int lapi_raw_next(lua_State *L, Hash *t, int i) {
    int tsize = nhash(L, t);
    for (; i < tsize; i++) {
        Node *n = node(L, t, i);
        if (ttype(val(L, n)) != LUA_T_NIL) {
            lapi_pushobject(L, ref(L, n));
            lapi_pushobject(L, val(L, n));
            return i + 1;  /* index to be used next time */
        }
    }
    return 0;  /* no more elements */
}

int lapi_next(lua_State *L, lua_Object o, int i) {
    TObject *t = lapi_address(L, o);
    if (ttype(t) != LUA_T_ARRAY)
        lua_error(L, "API error - object is not a table in `lua_next'");
    i = lapi_raw_next(L, avalue(t), i);
    lapi_top2L(L, (i == 0) ? 0 : 2);
    return i;
}