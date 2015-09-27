//
// Created by alex on 26/09/2015.
//
#include <Python.h>

#include <lua.h>
#include <stdbool.h>
#include "lauxlib.h"
#include "lualib.h"

#ifndef lua_next
#include "lapi.h"
#endif

#include "luaconv.h"
#include "pyconv.h"

int lua_isboolean(lua_State *L, lua_Object obj) {
    return lua_isuserdata(L, obj) && PyBool_Check((PyObject *) lua_getuserdata(L, obj));
}

int lua_getboolean(lua_State *L, lua_Object obj) {
    return PyObject_IsTrue((PyObject *) lua_getuserdata(L, obj));
}

int lua_gettop_c(lua_State *L) {
    return L->Cstack.num;
}

/*Base table object*/
int get_base_tag(lua_State *L) {
    lua_Object python = lua_getglobal(L, "python");
    lua_pushobject(L, python);
    lua_pushstring(L, POBJECT);
    return lua_tag(L, lua_gettable(L));
}

/*checks if a table contains only numbers*/
static int is_indexed_array(lua_State *L, lua_Object lobj) {
    lua_beginblock(L);
    int index = lua_next(L, lobj, 0);
    lua_Object key;
    while (index != 0) {
        key = lua_getparam(L, 1);
        if (!lua_isnumber(L, key))
            return 0;
        index = lua_next(L, lobj, index);
    }
    lua_endblock(L);
    return 1;
}

/* convert to args python: fn(*args) */
PyObject * _py_args(lua_State *L, lua_Object ltable, bool stacked, bool wrapped) {
    int nargs;
    if (stacked) {
        nargs = lua_gettop_c(L) - (wrapped ? 1 : 0);
    } else {
        lua_pushobject(L, ltable);
        lua_call(L, "getn");
        nargs = (int) lua_getnumber(L, lua_getresult(L, 1));
    }
    PyObject *args = PyTuple_New(nargs);
    if (!args) {
        PyErr_Print();
        lua_error(L, "failed to create arguments tuple");
    }
    if (stacked) {
        int i;
        for (i = 0; i != nargs; i++) {
            PyObject *arg = lua_convert(L, i + (wrapped ? 2 : 1));
            if (!arg) {
                Py_DECREF(args);
                char *error = "failed to convert argument #%d";
                char buff[strlen(error) + 10];
                sprintf(buff, error, i + 1);
                lua_error(L, &buff[0]);
            }
            PyTuple_SetItem(args, i, arg);
        }
    } else {
        lua_beginblock(L);
        PyObject *value;
        int index = lua_next(L, ltable, 0);

        while (index != 0) {
            value = lua_convert(L, 2);
            Py_INCREF(value);

            PyTuple_SetItem(args, index - 2, value);

            index = lua_next(L, ltable, index);
        }
        lua_endblock(L);
    }
    return args;
}

void py_args(lua_State *L) {
    PyObject *args = _py_args(L, 0, true, false);
    Py_INCREF(args);
    lua_pushuserdata(L, args);
}

/* convert to kwargs python: fn(**kwargs) */
PyObject *_py_kwargs(lua_State *L, lua_Object ltable) {
    PyObject *kwargs = PyDict_New();
    if (!kwargs) {
        PyErr_Print();
        lua_error(L, "failed to create key words arguments dict");
    }
    lua_beginblock(L);
    PyObject *key, *value;
    int index = lua_next(L, ltable, 0);

    while (index != 0) {
        key = lua_convert(L, 1);
        Py_INCREF(key);

        value = lua_convert(L, 2);
        Py_INCREF(value);

        PyDict_SetItem(kwargs, key, value);

        index = lua_next(L, ltable, index);
    }
    lua_endblock(L);
    return kwargs;
}

void py_kwargs(lua_State *L) {
    int nargs = lua_gettop_c(L);
    if (nargs < 1 || nargs > 1) {
        lua_error(L, "expected only one table");
    }

    lua_Object ltable = lua_getparam(L, 1);
    if (!lua_istable(L, ltable)) {
        lua_error(L, "first arg need be table ex: kwargs({a=10})");
    }

    PyObject *kwargs = _py_kwargs(L, ltable);
    Py_INCREF(kwargs);
    lua_pushuserdata(L, kwargs);
}

static py_object *get_py_object(lua_State *L, int n) {
    lua_Object ltable = lua_getparam(L, n);

    if (!lua_istable(L, ltable))
        lua_error(L, "wrap table not found!");

    py_object *po = (py_object *) malloc(sizeof(py_object));

    if (po == NULL)
        return NULL;

    // python object recover
    lua_pushobject(L, ltable);
    lua_pushstring(L, POBJECT);

    po->o = (PyObject *) lua_getuserdata(L, lua_rawgettable(L));

    lua_pushobject(L, ltable);
    lua_pushstring(L, ASINDX);

    po->asindx = (int) lua_getnumber(L, lua_rawgettable(L));

    return po;
}

PyObject *lua_convert(lua_State *L, int n) {
    PyObject *ret = NULL;
    lua_Object lobj = lua_getparam(L, n);
    if (lua_isnil(L, lobj)) {
        Py_INCREF(Py_None);
        ret = Py_None;
    } else if (lua_isnumber(L, lobj)) {
        double num = lua_getnumber(L, lobj);
        if (rintf((float) num) == num) {  // is int?
            ret = PyInt_FromLong((long) num);
        } else {
            ret = PyFloat_FromDouble(num);
        }
    } else if (lua_isstring(L, lobj)) {
        const char *s = lua_getstring(L, lobj);
        int len = lua_strlen(L, lobj);
        ret = PyString_FromStringAndSize(s, len);
        if (!ret) {
            ret = PyUnicode_FromStringAndSize(s, len);
        }
    } else if (lua_istable(L, lobj)) {
        if (get_base_tag(L) == lua_tag(L, lobj)) {
            py_object *pobj = get_py_object(L, n);
            ret = pobj->o;
            free(pobj);
        } else {
            if (is_indexed_array(L, lobj)) {
                ret = _py_args(L, lobj, false, false);
            } else {
                ret = _py_kwargs(L, lobj);
            }
        }
    } else if (lua_isboolean(L, lobj)) {
        if (lua_getboolean(L, lobj)) {
            Py_INCREF(Py_True);
            ret = Py_True;
        } else {
            Py_INCREF(Py_False);
            ret = Py_False;
        }
    } else if (lua_isuserdata(L, lobj)) {
        ret = (PyObject *) lua_getuserdata(L, lobj);
    } else if(lobj != 0) {
        ret = LuaObject_New(n);
    } else {
        ret = Py_None;
    }
    return ret;
}