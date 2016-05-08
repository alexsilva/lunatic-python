//
// Created by alex on 08/05/2016.
//

#include "lshared.h"

/* lua next optimized */
int lraw_next(lua_State *L, lua_Object lobj, int index, Node **n) {
    TObject *obj = lapi_address(L, lobj);
    Hash *hash = avalue(obj);
    int tsize = nhash(L, hash);
    while (index < tsize) {
        *n = node(L, hash, index);
        if (ttype(val(L, *n)) != LUA_T_NIL) {
            return index + 1;  /* index to be used next time */
        }
        index++;
    }
    return 0;  /* no more elements */
}