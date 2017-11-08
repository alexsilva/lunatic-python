//
// Created by alex on 27/09/2015.
//

#ifndef LUNATIC_UTILS_H
#define LUNATIC_UTILS_H
#include <stdbool.h>

#ifdef python_COMPAT
#define lua_traceback_checkerror(L) (0)
#define lua_traceback_message(L) ("")
#endif

typedef struct _lua {
    int tableref;
    bool tableconvert;   /* table convert */
    bool byref;          /* return strings python by reference */
    bool embedded;       /* is embedded   */
    int tag;
} Lua;

#define PY_UNICODE_MAX 16

typedef struct _python_unicode {
    char encoding[PY_UNICODE_MAX];
    char errorhandler[PY_UNICODE_MAX];
} PythonUnicode;

typedef struct _python {
    bool embedded;
    PythonUnicode *unicode;
    Lua *lua;
} Python;

Python *get_python(lua_State *L);

void set_python_api(lua_State *L, Python *python, char *name, lua_CFunction cfn);
void set_lua_api(lua_State *L, Lua *lua, char *name, void *udata);

void python_unicode_set_encoding(PythonUnicode *unicode, char *encoding);
void python_unicode_set_errorhandler(PythonUnicode *unicode, char *errorhandler);

PythonUnicode *python_unicode_init(lua_State *L);

Lua *lua_init(lua_State *L);
Python *python_init(lua_State *L);
void python_free(lua_State *L, Python *python);

lua_Object get_lua_bindtable(lua_State *L, Lua *lua);

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
void python_new_error(lua_State *L, PyObject *exception, char *message);
int lua_tablesize(lua_State *L, lua_Object ltable);
int python_api_tag(lua_State *L);

#ifndef strdup
char *strdup(const char *s);
#endif

int PyObject_IsListInstance(PyObject *obj);
int PyObject_IsTupleInstance(PyObject *obj);
int PyObject_IsDictInstance(PyObject *obj);

#define isvalidstatus(res) (((res) != UNCHANGED))

#define check_pyobject_index(pyObject) \
    (PyObject_IsListInstance(pyObject) || \
     PyObject_IsTupleInstance(pyObject) || \
     PyObject_IsDictInstance(pyObject))

#define is_byref(L) (get_python(L)->lua->byref)
#define set_byref(L, value) (get_python(L)->lua->byref = (value))

#endif //LUNATIC_UTILS_H

