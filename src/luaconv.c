//
// Created by alex on 26/09/2015.
//
#include <Python.h>

#include <lua.h>

#ifndef lua_next
#include "lapi.h"
#endif

#include "luaconv.h"
#include "pyconv.h"
#include "utils.h"
#include "constants.h"


PyObject *LuaObject_PyNew(InterpreterObject *interpreter, lua_Object lobj) {
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

PyObject *LuaObject_New(InterpreterObject *interpreter, int n) {
    return LuaObject_PyNew(interpreter, lua_getparam(interpreter->L, n));
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
    int index = lua_next(L, ltable, 0);
    lua_Object key;
    TObject *obj;
    double num;
    while (index != 0) {
        key = lua_getparam(L, 1);
        obj = lapi_address(L, key);
        switch (ttype(obj)) {
            case LUA_T_STRING:
                if (strcmp(lua_getstring(L, key), "n") != 0)
                    return false; // string key {"a" = 1} dict
                break;
            case LUA_T_NUMBER:
                num = lua_getnumber(L, key);
                if (rintf((float) num) != num)
                    return false; // float key {[2.5] = "a"} dict
                break;
            default:
                break; // int key {[1] = "a"} // list
        }
        index = lua_next(L, ltable, index);
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
    lua_Object larg;
    PyObject *arg;
    while (nextindex > 0) {
        larg = lua_getparam(L, stackpos);
        arg = lua_stack_convert(L, stackpos, larg);
        if (!arg) {
            Py_DECREF(tuple);
            char *format = "failed to convert argument #%d";
            char buff[strlen(format) + 32];
            sprintf(buff, format, index + 1);
            lua_error(L, &buff[0]);
        }
        if (is_object_container(L, larg))
            Py_INCREF(arg);  // “steals” a reference (arg is still valid in the Lua)
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
    lua_Object larg;
    PyObject *arg;
    while (nextindex > 0) {
        larg = lua_getparam(L, stackpos);
        arg = lua_stack_convert(L, stackpos, larg);
        if (!arg) {
            Py_DECREF(list);
            char *format = "failed to convert argument #%d";
            char buff[strlen(format) + 32];
            sprintf(buff, format, index + 1);
            lua_error(L, &buff[0]);
        }
        if (PyList_Append(list, arg) != 0) {
            if (!is_object_container(L, larg))
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
    lua_Object larg;
    for (index = 0; index != nargs; index++) {
        pos = index + stackpos + 1;
        larg = lua_getparam(L, pos);
        arg = lua_stack_convert(L, pos, larg);
        if (!arg) {
            Py_DECREF(tuple);
            char *format = "failed to convert argument #%d";
            char buff[strlen(format) + 32];
            sprintf(buff, format, index + 1);
            lua_new_error(L, &buff[0]);
        }
        if (is_object_container(L, larg))
            Py_INCREF(arg);  // “steals” a reference (arg is still valid in the Lua)
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
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 1);
    PyObject *tuple = get_py_tuple(L, 0);
    py_object *pobj = py_object_container(L, tuple, 1);
    lua_pushusertag(L, pobj, python_api_tag(L));
    pobj->isargs = true;
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 0);
}

/* Convert a table or a tuple for python args: fn(*args) */
void py_args_array(lua_State *L) {
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 1);
    lua_Object lobj = lua_getparam(L, 1);
    PyObject *obj;
    if (is_object_container(L, lobj)) {
        obj = get_pobject(L, lobj);
        // arguments must be tuple (conversion solves this)
        if (PyObject_IsListInstance(obj)) {  // tuple(list)
            obj = PyList_AsTuple(obj);
        } else if (!PyObject_IsTupleInstance(obj)) { // invalid type
            const char *repr = obj->ob_type->tp_name ? obj->ob_type->tp_name : "?";
            char *format = "object type \"%s\" can not be converted to args!";
            char buff[buffsize_calc(2, format, repr)];
            sprintf(buff, format, repr);
            lua_new_error(L, &buff[0]);
        }
    } else if (lua_istable(L, lobj)){
        obj = ltable_convert_tuple(L, lobj);
    } else {
        lua_new_error(L, "#1 table or list expected");
        return;
    }
    py_object *pobj = py_object_container(L, obj, 1);
    lua_pushusertag(L, pobj, python_api_tag(L));
    pobj->isargs = true;
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 0);
}

/* convert to kwargs python: fn(**kwargs) */
PyObject *get_py_dict(lua_State *L, lua_Object ltable) {
    PyObject *dict = PyDict_New();
    if (!dict) lua_new_error(L, "failed to create key words arguments dict");
    PyObject *key, *value;
    int index = lua_next(L, ltable, 0);
    lua_Object lkey, lvalue;
    int stackpos;
    while (index > 0) {
        stackpos = 1;
        lkey = lua_getparam(L, stackpos);
        key = lua_stack_convert(L, stackpos, lkey);
        if (!key) {
            Py_DECREF(dict);
            char *mkey = get_pyobject_str(key);
            char *skey = mkey ? mkey : "...";
            char *format = "failed to convert key \"%s\"";
            char buff[buffsize_calc(2, format, skey)];
            sprintf(buff, format, skey);
            free(mkey); // free pointer!
            lua_new_error(L, &buff[0]);
        }
        stackpos = 2;
        lvalue = lua_getparam(L, stackpos);
        value = lua_stack_convert(L, stackpos, lvalue);
        if (!value) {
            Py_DECREF(key);
            Py_DECREF(dict);
            char *mkey = get_pyobject_str(key);
            char *skey = mkey ? mkey : "...";
            char *format = "failed to convert value of key \"%s\"";
            char buff[buffsize_calc(2, format, skey)];
            sprintf(buff, format, skey);
            free(mkey); // free pointer!
            lua_new_error(L, &buff[0]);
        }
        if (PyDict_SetItem(dict, key, value) != 0) {
            Py_DECREF(key);
            Py_DECREF(value);
            Py_DECREF(dict);
            lua_new_error(L, "failed to set item in dict");
        }
        if (!is_object_container(L, lkey))
            Py_DECREF(key); // The key has no external references (will be deleted with the dict)
        if (!is_object_container(L, lvalue))
            Py_DECREF(value); // The value has no external references (will be deleted with the dict)
        index = lua_next(L, ltable, index);
    }
    return dict;
}

void py_kwargs(lua_State *L) {
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 1);
    int nargs = lua_gettop(L);
    if (nargs < 1 || nargs > 1) {
        lua_error(L, "expected only one table");
    }
    lua_Object ltable = lua_getparam(L, 1);
    if (!lua_istable(L, ltable)) {
        lua_error(L, "first arg need be table ex: pykwargs{a=10}");
    }
    PyObject *dict = get_py_dict(L, ltable);
    py_object *pobj = py_object_container(L, dict, 1);
    lua_pushusertag(L, pobj, python_api_tag(L));
    pobj->iskwargs = true;
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 0);
}

/**
 * Returns the object python inside the container.
**/
PyObject *get_pobject(lua_State *L, lua_Object userdata) {
    if (!is_object_container(L, userdata))
        lua_error(L, "#1 container for invalid pyobject!");
    return ((py_object *) lua_getuserdata(L, userdata))->object;
}

/**
 * Returns the pointer stored by the Lua object.
**/
py_object *get_py_object(lua_State *L, lua_Object userdata) {
    if (!is_object_container(L, userdata))
        lua_error(L, "#2 container for invalid pyobject!");
    return ((py_object *) lua_getuserdata(L, userdata));
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
    if (!python_getnumber(interpreter->L, PY_API_IS_EMBEDDED) && // Lua inside Python
        !python_getnumber(interpreter->L, PY_LUA_TABLE_CONVERT)){
        *ret = LuaObject_PyNew(interpreter, lobj);
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
        } else if (python_getnumber(interpreter->L, PY_API_IS_EMBEDDED)) { //  Python inside Lua
            *ret = (PyObject *) void_ptr;
        } else {
            *ret = LuaObject_PyNew(interpreter, lobj);
        }
    }  else {
        Py_INCREF(Py_None);
        *ret = Py_None;
    }
}

PyObject *lua_interpreter_object_convert(InterpreterObject *interpreter, int stackpos,
                                         lua_Object lobj) {
    PyObject *ret = NULL;
    if (lobj == LUA_NOOBJECT) lobj = lua_getparam(interpreter->L, stackpos);
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
            ret = LuaObject_PyNew(interpreter, lobj);
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

PyObject *lua_stack_convert(lua_State *L, int stackpos, lua_Object lobj) {
    InterpreterObject interpreter;
    interpreter.isPyType = false;
    interpreter.L = L;
    return lua_interpreter_object_convert(&interpreter, stackpos, lobj);
}

PyObject *lua_interpreter_stack_convert(InterpreterObject *interpreter,
                                         int stackpos) {
    return lua_interpreter_object_convert(interpreter, stackpos,
                                          lua_getparam(interpreter->L, stackpos));
}

PyObject *lua_convert(lua_State *L, int stackpos) {
    return lua_stack_convert(L, stackpos, lua_getparam(L, stackpos));
}