//
// Created by alex on 26/09/2015.
//
#include <Python.h>

#include <lua.h>
#include <lauxlib.h>

#if defined(_WIN32)
#include "lapi.h"
#else
// also included in lapi
#include "lshared.h"
#endif

#include "luaconv.h"
#include "pyconv.h"
#include "utils.h"
#include "constants.h"
#include "luainpython.h"


PyObject *LuaObject_New(InterpreterObject *interpreter, lua_Object lobj) {
    LuaObject *obj = PyObject_New(LuaObject, &LuaObject_Type);
    if (obj) {
        lua_pushobject(interpreter->L, lobj);
        obj->ref = lua_ref(interpreter->L, 1);
        obj->indexed = lua_istable(interpreter->L, lobj) ? is_indexed_array(interpreter->L, lobj) : false;
        obj->refiter = 0;
        if (interpreter->isPyType) {
            Py_INCREF(interpreter);
            // The state of the Lua will be used implicitly.
            obj->interpreter = interpreter;
        } else {
            lua_State *L = interpreter->L;
            obj->interpreter = malloc(sizeof(InterpreterObject));
            if (!obj->interpreter) lua_error(L, "failed to allocate memory for the interpreter!");
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
            obj->interpreter->L = L;
            obj->interpreter->isPyType = false;  // fake type
#pragma clang diagnostic pop
        }
    }
    return (PyObject*) obj;
}

/**
 * Returns the number of objects on the Lua stack.
**/
int lua_gettop(lua_State *L) {
    return L->Cstack.num;
}

/**
 * Checks whether the object is a Lua userdata containing the tag event base.
 **/
int is_object_container(lua_State *L, lua_Object lobj) {
    return lua_isuserdata(L, lobj) && lua_tag(L, lobj) == python_api_tag(L);
}

/**
 * Verifies that container (userdata) is a tuple with arguments of function fn(*args)
 * return [bool] true|false
**/
bool ispyargs(lua_State *L, lua_Object userdata) {
    return is_object_container(L, userdata) ? get_py_object(L, userdata)->isargs : false;
}

/**
 * Verifies that container (userdata) is a dictionary of keyword arguments of function fn(**kwargs)
 * return [bool] true|false
**/
bool ispykwargs(lua_State *L, lua_Object userdata) {
    return is_object_container(L, userdata) ? get_py_object(L, userdata)->iskwargs : false;
}

/* Checks if a table contains only numbers as keys */
bool is_indexed_array(lua_State *L, lua_Object ltable) {
    double num;
    Node *n;
    TObject *key;
    int index = 0;
    while ((index = lraw_next(L, ltable, index, &n)) > 0) {
        key = lua_getkey(L, n);
        switch (lua_gettype(key)) {
            case LUA_T_STRING:
                if (strcmp(lua_getstr(key), "n") != 0)
                    return false; // string key {"a" = 1} dict
                break;
            case LUA_T_NUMBER:
                num = lua_getnum(key);
                if (rint(num) != num)
                    return false; // float key {[2.5] = "a"} dict
                break;
            default:
                break; // int key {[1] = "a"} // list
        }
    }
    return true;
}

/**
 * Convert a lua table for python tuple
 **/
PyObject *ltable_convert_tuple(lua_State *L, lua_Object ltable) {
    int nargs = lua_tablesize(L, ltable);
    PyObject *tuple = PyTuple_New(nargs);
    if (!tuple) lua_new_error(L, "#4 failed to create arguments tuple");
    set_table_nil(L, ltable, "n"); // remove "n"
    int nextindex = lua_next(L, ltable, 0);
    int index = 0, stackpos = 2;
    PyObject *arg;
    while (nextindex > 0) {
        arg = lua_stack_convert(L, stackpos);
        if (!arg) {
            Py_DECREF(tuple);
            char *format = "failed to convert argument #%d";
            char buff[strlen(format) + 32];
            sprintf(buff, format, index + 1);
            lua_new_error(L, &buff[0]);
        }
        if (PyTuple_SetItem(tuple, index, arg) != 0) {
            Py_XDECREF(arg);
            Py_DECREF(tuple);
            lua_new_error(L, "failed to set item in tuple");
        }
        nextindex = lua_next(L, ltable, nextindex);
        index++;
    }
    return tuple;
}

/**
 * Convert a lua table for python tuple
 **/
PyObject *ltable2list(lua_State *L, lua_Object ltable) {
    PyObject *list = PyList_New(0);
    if (!list) lua_new_error(L, "failed to create list");
    set_table_nil(L, ltable, "n"); // remove "n"
    int nextindex = lua_next(L, ltable, 0);
    int index = 0, stackpos = 2;
    PyObject *arg;
    while (nextindex > 0) {
        arg = lua_stack_convert(L, stackpos);
        if (!arg) {
            Py_DECREF(list);
            char *format = "failed to convert argument #%d";
            char buff[strlen(format) + 32];
            sprintf(buff, format, index + 1);
            lua_new_error(L, &buff[0]);
        }
        if (PyList_Append(list, arg) != 0) {
            Py_DECREF(arg);
            Py_DECREF(list);
            lua_new_error(L, "failed to set item in list");
        }
        nextindex = lua_next(L, ltable, nextindex);
        index++;
    }
    return list;
}

/* Convert arguments in the stack lua to tuple */
PyObject *get_py_tuple(lua_State *L, int stackpos) {
    int nargs = lua_gettop(L) - stackpos;
    PyObject *tuple = PyTuple_New(nargs);
    if (!tuple) lua_new_error(L, "#2 failed to create arguments tuple");
    int index, pos;
    PyObject *arg;
    for (index = 0; index < nargs; index++) {
        pos = index + stackpos + 1;
        arg = lua_stack_convert(L, pos);
        if (!arg) {
            Py_DECREF(tuple);
            char *format = "failed to convert argument #%d";
            char buff[strlen(format) + 32];
            sprintf(buff, format, index + 1);
            lua_new_error(L, &buff[0]);
        }
        if (PyTuple_SetItem(tuple, index, arg) != 0) {
            Py_XDECREF(arg);
            Py_DECREF(tuple);
            lua_new_error(L, "failed to set item in tuple");
        }
    }
    return tuple;
}

/* Converts the list of arguments in the stack for python args: fn(*args) */
void py_args(lua_State *L) {
    Python *python = get_python(L);
    python->lua->tableconvert = true;
    PyObject *tuple = get_py_tuple(L, 0);
    py_object *pobj = py_object_container(L, tuple, 1);
    lua_pushusertag(L, pobj, python_api_tag(L));
    pobj->isargs = true;
    python->lua->tableconvert = false;
}

// raise key error in lua
static void raise_key_error(lua_State *L, char *format,
                            lua_Object lkey) {
    char *mkey;
    int is_object = is_object_container(L, lkey);
    if (is_object) {
        mkey = get_pyobject_str(get_pobject(L, lkey));
    } else {
        lua_pushobject(L, lkey);
        lua_call(L, "tostring");
        mkey = lua_getstring(L, lua_getparam(L, 1));
    }
    char *skey = mkey ? mkey : "?";
    char buff[buffsize_calc(2, format, skey)];
    sprintf(buff, format, skey);
    if (is_object) free(mkey);
    lua_new_error(L, &buff[0]);
}


/* convert to kwargs python: fn(**kwargs) */
PyObject *get_py_dict(lua_State *L, lua_Object ltable) {
    PyObject *dict = PyDict_New();
    if (!dict) lua_new_error(L, "failed to create key words arguments dict");
    PyObject *key, *value;
    int index = lua_next(L, ltable, 0);
    int stackpos;
    lua_Object lkey;
    while (index > 0) {
        stackpos = 1;
        lkey = lua_getparam(L, stackpos);
        key = lua_object_convert(L, lkey);
        if (!key) {
            Py_DECREF(dict);
            raise_key_error(L, "failed to convert key \"%s\"", lkey);
        }
        stackpos = 2;
        value = lua_stack_convert(L, stackpos);
        if (!value) {
            Py_DECREF(key);
            Py_DECREF(dict);
            raise_key_error(L, "failed to convert value of key \"%s\"", lkey);
        }
        if (PyDict_SetItem(dict, key, value) != 0) {
            Py_DECREF(key);
            Py_DECREF(value);
            Py_DECREF(dict);
            lua_new_error(L, "failed to set item in dict");
        } else {
            Py_DECREF(key);
            Py_DECREF(value);
        }
        index = lua_next(L, ltable, index);
    }
    return dict;
}

void py_kwargs(lua_State *L) {
    Python *python = get_python(L);
    python->lua->tableconvert = true;
    PyObject *dict = get_py_dict(L, luaL_tablearg(L, 1));
    py_object *pobj = py_object_container(L, dict, 1);
    pobj->iskwargs = true;
    lua_pushusertag(L, pobj, python_api_tag(L));
    python->lua->tableconvert = false;
}

/**
 * Returns the pointer stored by the Lua object.
**/
py_object *get_py_object(lua_State *L, lua_Object userdata) {
    if (!is_object_container(L, userdata))
        lua_error(L, "container for invalid pyobject!");
    return ((py_object *) lua_getuserdata(L, userdata));
}

/**
 * Returns the object python inside the container.
**/
PyObject *get_pobject(lua_State *L, lua_Object userdata) {
    return (get_py_object(L, userdata))->object;
}

static void lnumber_convert(InterpreterObject *interpreter, lua_Object lobj, PyObject **ret) {
    double num = lua_getnumber(interpreter->L, lobj);
    if (rintf((float) num) == num) {  // is int?
        *ret = PyInt_FromLong((long) num);
    } else {
        *ret = PyFloat_FromDouble(num);
    }
}

static void lstring_convert(InterpreterObject *interpreter, lua_Object lobj, PyObject **ret) {
    const char *s = lua_getstring(interpreter->L, lobj);
    int len = lua_strlen(interpreter->L, lobj);
    *ret = PyString_FromStringAndSize(s, len);
}

static void ltable_convert(InterpreterObject *interpreter, lua_Object lobj, PyObject **ret) {
    lua_beginblock(interpreter->L);
    Python *python = get_python(interpreter->L);
    if (python->lua->embedded && !python->lua->tableconvert) { // Lua inside Python
        *ret = LuaObject_New(interpreter, lobj);
    } else if (is_indexed_array(interpreter->L, lobj)) { //  Python inside Lua
        *ret = ltable_convert_tuple(interpreter->L, lobj);
    } else {
        *ret = get_py_dict(interpreter->L, lobj);
    }
    lua_endblock(interpreter->L);
}

static void luserdata_convert(InterpreterObject *interpreter, lua_Object lobj, PyObject **ret) {
    void *void_ptr = lua_getuserdata(interpreter->L, lobj); // userdata NULL ?
    if (void_ptr) {
        if (is_object_container(interpreter->L, lobj)) {
            *ret = get_pobject(interpreter->L, lobj);
            Py_INCREF(*ret); // new ref
        } else if (get_python(interpreter->L)->embedded) { //  Python inside Lua
            *ret = (PyObject *) void_ptr;
        } else {
            *ret = LuaObject_New(interpreter, lobj);
        }
    }  else {
        Py_INCREF(Py_None);
        *ret = Py_None;
    }
}

PyObject *lua_interpreter_object_convert(InterpreterObject *interpreter,
                                         lua_Object lobj) {
    PyObject *ret = NULL;
    TObject *o = lapi_address(interpreter->L, lobj);
    switch (ttype(o)) { // Lua 3.2 source code builtin.c
        case LUA_T_NUMBER:
            lnumber_convert(interpreter, lobj, &ret);
            break;
        case LUA_T_STRING:
            lstring_convert(interpreter, lobj, &ret);
            break;
        case LUA_T_ARRAY:  // lus_istable
            ltable_convert(interpreter, lobj, &ret);
            break;
        case LUA_T_CLOSURE: // lua_isfunction
        case LUA_T_PROTO:
        case LUA_T_CPROTO:
            ret = LuaObject_New(interpreter, lobj);
            break;
        case LUA_T_USERDATA:
            luserdata_convert(interpreter, lobj, &ret);
            break;
        case LUA_T_NIL:
            Py_INCREF(Py_None);
            ret = Py_None;
            break;
        default:
            lua_error(interpreter->L, "unknown type!");
    }
    return ret;
}

PyObject *lua_interpreter_stack_convert(InterpreterObject *interpreter, int stackpos) {
    return lua_interpreter_object_convert(interpreter, lua_getparam(interpreter->L, stackpos));
}

PyObject *lua_object_convert(lua_State *L, lua_Object lobj) {
    InterpreterObject interpreter;
    interpreter.isPyType = false;
    interpreter.L = L;
    return lua_interpreter_object_convert(&interpreter, lobj);
}

PyObject *lua_stack_convert(lua_State *L, int stackpos) {
    return lua_object_convert(L, lua_getparam(L, stackpos));
}
