//
// Created by alex on 27/09/2015.
//
struct JmpError {
    jmp_buf buff;
};

#ifndef LUNATIC_UTILS_H
#define LUNATIC_UTILS_H

/* A generic macro to insert a value in a Lua table. */
#define insert_table(L, table, index, value, type) \
  { \
    lua_pushobject( L, table ); \
    lua_pushstring( L, index ); \
    lua_push##type( L, value ); \
    lua_settable( L );    \
  }


/* set userdata */
#define set_table_userdata(L, ltable, name, udata)\
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushuserdata(L, udata);\
    lua_rawsettable(L);

/* set userdata */
#define set_table_usertag(L, ltable, name, udata, ntag)\
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushusertag(L, udata, ntag);\
    lua_rawsettable(L);

/* set number */
#define set_table_number(L, ltable, name, number)\
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushnumber(L, number);\
    lua_rawsettable(L);

/* set function */
#define set_table_fn(L, ltable, name, fn)\
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushcfunction(L, fn);\
    lua_rawsettable(L);

/* set object */
#define set_table_object(L, ltable, name, obj) \
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushobject(L, obj);\
    lua_rawsettable(L);

/* set object */
#define set_table_string(L, ltable, name, str) \
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushstring(L, str);\
    lua_rawsettable(L);

/* set object */
#define set_table_nil(L, ltable, name) \
    lua_pushobject(L, ltable);\
    lua_pushstring(L, name);\
    lua_pushnil(L);\
    lua_rawsettable(L);

int buffsize_calc(int nargs, ...);
void lua_new_error(lua_State *L, char *message);
void lua_raise_error(lua_State *L, char *format, PyObject *obj);
char *get_pyobject_str(PyObject *obj);
void python_new_error(PyObject *exception, char *message);
int python_getnumber(lua_State *L, char *name);
char *python_getstring(lua_State *L, char *name);
void *python_getuserdata(lua_State *L, char *name);
int python_try(lua_State *L); void python_catch(lua_State *L);
void python_setstring(lua_State *L, char *name, char *value);
void python_setnumber(lua_State *L, char *name, int value);
int lua_tablesize(lua_State *L, lua_Object ltable);
int python_api_tag(lua_State *L);

#ifndef strdup
char *strdup(const char *s);
#endif

int PyObject_IsListInstance(PyObject *obj);
int PyObject_IsTupleInstance(PyObject *obj);
int PyObject_IsDictInstance(PyObject *obj);

#define isvalidstatus(res) ((res != UNCHANGED))

#define check_pyobject_index(pyObject) \
    (PyObject_IsListInstance(pyObject) || \
     PyObject_IsTupleInstance(pyObject) || \
     PyObject_IsDictInstance(pyObject))

#endif //LUNATIC_UTILS_H

#define is_byref(L) python_getnumber(L, PY_OBJECT_BY_REFERENCE)
#define set_byref(L, value) python_setnumber(L, PY_OBJECT_BY_REFERENCE, value)

