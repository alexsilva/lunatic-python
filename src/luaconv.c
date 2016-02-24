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
#include "pythoninlua.h"
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
            if (!obj->interpreter) lua_error(L, "out of memory!");
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
            obj->interpreter->L = L;
            obj->interpreter->isPyType = false;  // fake type
            obj->interpreter->allocated = true;
#pragma clang diagnostic pop
        }

    }
    return (PyObject*) obj;
}

PyObject *LuaObject_New(InterpreterObject *interpreter, int n) {
    return LuaObject_PyNew(interpreter, lua_getparam(interpreter->L, n));
}

int lua_gettop(lua_State *L) {
    return L->Cstack.num;
}

/*Base table object*/
int get_base_tag(lua_State *L) {
    lua_Object python = lua_getglobal(L, PYTHON_API);
    lua_pushobject(L, python);
    lua_pushstring(L, PY_OBJECT);
    return lua_tag(L, lua_gettable(L));
}

static int getnumber(lua_State *L, char *name, lua_Object ltable) {
    lua_pushobject(L, ltable);
    lua_pushstring(L, name);
    return (int) lua_getnumber(L, lua_rawgettable(L));
}

static int is_wrap_base(lua_State *L, lua_Object lobj) {
    return getnumber(L, LUA_BASE_TAG, lobj);
}

int is_wrapped_object(lua_State *L, lua_Object lobj) {
    return lua_istable(L, lobj) && get_base_tag(L) == lua_tag(L, lobj) && !is_wrap_base(L, lobj);
}

int is_wrapped_args(lua_State *L, lua_Object lwtable) {
    return is_wrapped_object(L, lwtable) && getnumber(L, PY_ARGS_WRAP, lwtable);
}

int is_wrapped_kwargs(lua_State *L, lua_Object lwtable) {
    return is_wrapped_object(L, lwtable) && getnumber(L, PY_KWARGS_WRAP, lwtable);
}

/* Checks if a table contains only numbers as keys */
bool is_indexed_array(lua_State *L, lua_Object ltable) {
    int index = lua_next(L, ltable, 0);
    lua_Object key;
    while (index != 0) {
        key = lua_getparam(L, 1);
        if ((!lua_isnumber(L, key) && lua_isstring(L, key) &&
             strcmp(lua_getstring(L, key), "n") != 0))
            return false;
        index = lua_next(L, ltable, index);
    }
    return true;
}

/* Convert a lua table for python tuple */
PyObject *_get_py_tuple(lua_State *L, lua_Object ltable) {
    lua_pushobject(L, ltable);
    lua_call(L, "getn");
    int nargs = (int) lua_getnumber(L, lua_getresult(L, 1));
    PyObject *tuple = PyTuple_New(nargs);
    if (!tuple) lua_new_error(L, "failed to create arguments tuple");
    set_table_nil(L, ltable, "n"); // remove "n"
    int index = lua_next(L, ltable, 0);
    int count = 0, stackpos = 2;
    lua_Object larg;
    PyObject *arg;
    while (index != 0) {
        larg = lua_getparam(L, stackpos);
        arg = lua_stack_convert(L, stackpos, larg);
        if (is_wrapped_object(L, larg))
            Py_INCREF(arg);  // “steals” a reference (arg is still valid in the Lua)
        if (PyTuple_SetItem(tuple, count, arg) != 0) {
            Py_XDECREF(arg);
            Py_DECREF(tuple);
            lua_new_error(L, "failed to set item in tuple");
        }
        index = lua_next(L, ltable, index);
        count++;
    }
    return tuple;
}

/* Convert arguments in the stack lua to tuple */
PyObject *get_py_tuple(lua_State *L, int stackpos) {
    int nargs = lua_gettop(L) - stackpos;
    PyObject *tuple = PyTuple_New(nargs);
    if (!tuple) lua_new_error(L, "failed to create arguments tuple");
    int index, pos;
    PyObject *arg;
    lua_Object larg;
    for (index = 0; index != nargs; index++) {
        pos = index + stackpos + 1;
        larg = lua_getparam(L, pos);
        arg = lua_stack_convert(L, pos, larg);
        if (!arg) {
            Py_DECREF(tuple);
            char *error = "failed to convert argument #%d";
            char buff[strlen(error) + 10];
            sprintf(buff, error, index + 1);
            lua_error(L, &buff[0]);
        }
        if (is_wrapped_object(L, larg))
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
    PyObject *tuple = get_py_tuple(L, 0);
    lua_Object lwtable = py_object_wrapped(L, tuple, 1);
    set_table_number(L, lwtable, PY_ARGS_WRAP, 1);
    lua_pushobject(L, lwtable); // returning table
}

/* convert a table or a tuple for python args: fn(*args) */
void py_args_array(lua_State *L) {
    lua_Object lobj = lua_getparam(L, 1);
    PyObject *pobj;
    if (is_wrapped_object(L, lobj)) {
        pobj = get_pobject(L, lobj);
    } else {
        pobj = _get_py_tuple(L, lobj);
    }
    lua_Object lwtable = py_object_wrapped(L, pobj, 1);
    set_table_number(L, lwtable, PY_ARGS_WRAP, 1);
    lua_pushobject(L, lwtable); // returning table
}


/* convert to kwargs python: fn(**kwargs) */
PyObject *get_py_dict(lua_State *L, lua_Object ltable) {
    PyObject *dict = PyDict_New();
    if (!dict) lua_new_error(L, "failed to create key words arguments dict");
    PyObject *key, *value;
    int index = lua_next(L, ltable, 0);
    lua_Object lkey, lvalue;
    int stackpos;
    while (index != 0) {
        stackpos = 1;
        lkey = lua_getparam(L, stackpos);
        key = lua_stack_convert(L, stackpos, lkey);
        stackpos = 2;
        lvalue = lua_getparam(L, stackpos);
        value = lua_stack_convert(L, stackpos, lvalue);
        if (PyDict_SetItem(dict, key, value) != 0) {
            Py_XDECREF(key);
            Py_XDECREF(value);
            Py_DECREF(dict);
            lua_new_error(L, "failed to set item in dict");
        }
        if (!is_wrapped_object(L, lkey))
            Py_DECREF(key); // The key has no external references (will be deleted with the dict)
        if (!is_wrapped_object(L, lvalue))
            Py_DECREF(value); // The value has no external references (will be deleted with the dict)
        index = lua_next(L, ltable, index);
    }
    return dict;
}

void py_kwargs(lua_State *L) {
    int nargs = lua_gettop(L);
    if (nargs < 1 || nargs > 1) {
        lua_error(L, "expected only one table");
    }
    lua_Object ltable = lua_getparam(L, 1);
    if (!lua_istable(L, ltable)) {
        lua_error(L, "first arg need be table ex: pykwargs{a=10}");
    }
    PyObject *dict = get_py_dict(L, ltable);
    lua_Object lwtable = py_object_wrapped(L, dict, 1);
    set_table_number(L, lwtable, PY_KWARGS_WRAP, 1);
    lua_pushobject(L, lwtable); // returning table
}

/*get py object from wrap table (direct access) */
PyObject *get_pobject(lua_State *L, lua_Object ltable) {
    if (!is_wrapped_object(L, ltable))
        lua_error(L, "wrap table not found!");

    // python object recover
    lua_pushobject(L, ltable);
    lua_pushstring(L, PY_OBJECT);

    return lua_getuserdata(L, lua_rawgettable(L));
}

/*get py object from wrap table */
py_object *get_py_object(lua_State *L, lua_Object ltable) {
    if (!is_wrapped_object(L, ltable))
        lua_error(L, "wrap table not found!");

    py_object *po = (py_object *) malloc(sizeof(py_object));
    if (po == NULL) {
        lua_error(L, "out of memory!");
    }
    // python object recover
    lua_pushobject(L, ltable);
    lua_pushstring(L, PY_OBJECT);

    po->o = (PyObject *) lua_getuserdata(L, lua_rawgettable(L));

    lua_pushobject(L, ltable);
    lua_pushstring(L, PY_OBJECT_AS_INDEX);

    po->asindx = (int) lua_getnumber(L, lua_rawgettable(L));

    // python object recover
    lua_pushobject(L, ltable);
    lua_pushstring(L, PY_OBJECT_META);

    po->meta = (py_object_meta *) lua_getuserdata(L, lua_rawgettable(L));
    return po;
}

/* get py object in stack */
py_object *get_py_object_stack(lua_State *L, int n) {
    return get_py_object(L, lua_getparam(L, n));
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
    if (is_wrapped_object(interpreter->L, lobj)) {
        *ret = get_pobject(interpreter->L, lobj);
    } else {
        lua_beginblock(interpreter->L);
        if (!PYTHON_EMBEDDED_MODE) { // Lua inside Python
            *ret = LuaObject_PyNew(interpreter, lobj);
        } else if (is_indexed_array(interpreter->L, lobj)) { //  Python inside Lua
            *ret = _get_py_tuple(interpreter->L, lobj);
        } else {
            *ret = get_py_dict(interpreter->L, lobj);
        }
        lua_endblock(interpreter->L);
    }
}

static void luserdata_convert(InterpreterObject *interpreter, lua_Object lobj, PyObject **ret) {
    void *void_ptr = lua_getuserdata(interpreter->L, lobj); // userdata NULL ?
    if (void_ptr) {
        if (PYTHON_EMBEDDED_MODE) {
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