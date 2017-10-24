//
// Created by alex on 27/09/2015.
//

#ifndef LUNATIC_UTILS_H
#define LUNATIC_UTILS_H
extern "C"
{
#include "lua.h"
#include "Python.h"
}
#include "stack.h"

#define ptrchar (char*)

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
void lua_new_argerror (lua_State *L, int numarg, char *extramsg);
void lua_raise_error(lua_State *L, char *format, PyObject *obj);
char *get_pyobject_str(PyObject *obj);
void python_new_error(PyObject *exception, char *message);
int lua_tablesize(lua_State *L, lua_Object ltable);
int python_api_tag(lua_State *L);

#ifndef strdup
char *strdup(const char *s);
#endif

int PyObject_IsListInstance(PyObject *obj);
int PyObject_IsTupleInstance(PyObject *obj);
int PyObject_IsDictInstance(PyObject *obj);

#define isvalidstatus(res) ((res) != UNCHANGED)

#define check_pyobject_index(pyObject) \
    (PyObject_IsListInstance(pyObject) || \
     PyObject_IsTupleInstance(pyObject) || \
     PyObject_IsDictInstance(pyObject))

#define begintry { try {
#define endcatch } catch (int e) {} catch (...) {} }


class PyUnicode {
public:
    PyUnicode() {
        strcpy(encoding, ptrchar "UTF8");
        strcpy(errorhandler, ptrchar "strict");
    }
    ~PyUnicode(){}
    char encoding[255]{};
    char errorhandler[255]{};
};

class Lua {
public:
    explicit Lua(lua_State *L){
        tag = lua_newtag(L);
        ref = new_data(L);
    }
    bool tableconvert = false;           /* table convert */
    bool embedded     = false;           /* is embedded   */
    lua_Object get_datatable(lua_State *L) {
        lua_Object lua_object = lua_getref(L, ref);
        if (!lua_istable(L, lua_object)) {
            lua_new_error(L, ptrchar "lost reference!");
        }
        return lua_object;
    }
    lua_Object get(lua_State *L, const char *name) {  /* table of data */
        lua_pushobject(L, get_datatable(L));
        lua_pushstring(L, (char *) name);
        return lua_rawgettable(L);
    }
    void set_api(lua_State *L, const char *name, void *udata) {
        lua_pushobject(L, get_datatable(L));
        lua_pushstring(L, (char*) name);
        lua_pushusertag(L, udata, tag);
        lua_rawsettable(L);
    }
    void set(lua_State *L, const char *name, lua_Object lua_object) {
        lua_pushobject(L, get_datatable(L));
        lua_pushstring(L, (char*) name);
        lua_pushobject(L, lua_object);
        lua_rawsettable(L);
    }
    int get_tag() {
        return this->tag;
    }
    int get_ref() {
        return this->ref;
    }
protected:
    int ref; /* data section  */
    int tag; /* api tag */
private:
    int new_data(lua_State *L) {
        lua_pushobject(L, lua_createtable(L));
        return lua_ref(L, 1);
    }
};

class Python {
public:
    explicit Python(lua_State *L);
    ~Python();
    PyUnicode *unicode;
    bool object_ref;
    bool embedded;
    STACK stack;
    Lua lua;
};


Python *get_python(lua_State *L);
#endif //LUNATIC_UTILS_H
