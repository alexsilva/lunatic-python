//
// Created by alex on 27/09/2015.
//

#ifndef LUNATIC_UTILS_H
#define LUNATIC_UTILS_H

/* set userdata */
#define set_table_userdata(L, ltable, name, udata)\
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushuserdata(L, udata);\
    lua_settable(L);

/* set number */
#define set_table_number(L, ltable, name, number)\
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushnumber(L, number);\
    lua_settable(L);

/* set function */
#define set_table_fn(L, ltable, name, fn)\
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushcfunction(L, fn);\
    lua_settable(L);

/* set function */
#define set_table_object(L, ltable, name, obj) \
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushobject(L, obj);\
    lua_settable(L);

int calc_buff_size(int nargs, ...);
void lua_new_error(lua_State *L, char *message);
char *get_pyobject_str(PyObject *pyobject, char *dftstr);

#endif //LUNATIC_UTILS_H
