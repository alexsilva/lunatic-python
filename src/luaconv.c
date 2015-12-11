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
#include "luainpython.h"


PyObject *LuaObject_PyNew(InterpreterObject *interpreter, lua_Object lobj) {
    LuaObject *obj = PyObject_New(LuaObject, &LuaObject_Type);
    if (obj) {
        lua_pushobject(interpreter->L, lobj);
        obj->ref = lua_ref(interpreter->L, 1);
        obj->refiter = 0;
        if (!interpreter->malloc) Py_INCREF(interpreter);
        obj->interpreter = interpreter; // The state of the Lua will be used implicitly.
        obj->L= interpreter->L;
        interpreter->link = true;
    }
    return (PyObject*) obj;
}

PyObject *LuaObject_New(InterpreterObject *interpreter, int n) {
    return LuaObject_PyNew(interpreter, lua_getparam(interpreter->L, n));
}

int lua_isboolean(lua_State *L, lua_Object obj) {
    if (lua_isuserdata(L, obj) && lua_getuserdata(L, obj))
        return PyBool_Check((PyObject *) lua_getuserdata(L, obj));
    return 0;
}

int lua_getboolean(lua_State *L, lua_Object obj) {
    return PyObject_IsTrue((PyObject *) lua_getuserdata(L, obj));
}

int lua_gettop(lua_State *L) {
    return L->Cstack.num;
}

/*Base table object*/
int get_base_tag(lua_State *L) {
    lua_Object python = lua_getglobal(L, "python");
    lua_pushobject(L, python);
    lua_pushstring(L, POBJECT);
    return lua_tag(L, lua_gettable(L));
}

static int is_wrap_base(lua_State *L, lua_Object lobj) {
    lua_pushobject(L, lobj);
    lua_pushstring(L, "base");
    lua_Object base = lua_rawgettable(L);
    return lua_isboolean(L, base) && lua_getboolean(L, base);
}

int is_wrapped_object(lua_State *L, lua_Object lobj) {
    return lua_istable(L, lobj) && get_base_tag(L) == lua_tag(L, lobj) && !is_wrap_base(L, lobj);
}

/*checks if a table contains only numbers*/
int is_indexed_array(lua_State *L, lua_Object lobj) {
    int index = lua_next(L, lobj, 0);
    lua_Object key;
    while (index != 0) {
        key = lua_getparam(L, 1);
        if (!lua_isnumber(L, key))
            return 0;
        index = lua_next(L, lobj, index);
    }
    return 1;
}

/* convert to args python: fn(*args) */
PyObject *_get_py_tuple(lua_State *L, lua_Object ltable) {
    lua_pushobject(L, ltable);
    lua_call(L, "getn");
    int nargs = (int) lua_getnumber(L, lua_getresult(L, 1));
    PyObject *tuple = PyTuple_New(nargs);
    if (!tuple) {
        lua_new_error(L, "failed to create arguments tuple");
    }
    PyObject *value;
    int index = lua_next(L, ltable, 0);
    while (index != 0) {
        value = lua_convert(L, 2);
        PyTuple_SetItem(tuple, index - 2, value);
        index = lua_next(L, ltable, index);
    }
    return tuple;
}

/* convert to args python: fn(*args) */
PyObject *get_py_tuple(lua_State *L, int stackpos) {
    int nargs = lua_gettop(L) - stackpos;
    PyObject *tuple = PyTuple_New(nargs);
    if (!tuple) {
        lua_new_error(L, "failed to create arguments tuple");
    }
    int i, pos;
    PyObject *arg;
    for (i = 0; i != nargs; i++) {
        pos = i + stackpos + 1;
        arg = lua_convert(L, pos);
        if (!arg) {
            Py_DECREF(tuple);
            char *error = "failed to convert argument #%d";
            char buff[strlen(error) + 10];
            sprintf(buff, error, i + 1);
            lua_error(L, &buff[0]);
        }
        if (is_wrapped_object(L, lua_getparam(L, pos)))
            Py_INCREF(arg);  // “steals” a reference (arg is still valid in the Lua)
        PyTuple_SetItem(tuple, i, arg);
    }
    return tuple;
}

void py_args(lua_State *L) {
    PyObject *tuple = get_py_tuple(L, 0);
    py_object_wrap_lua(L, tuple, 1);
}

/* convert to kwargs python: fn(**kwargs) */
PyObject *get_py_dict(lua_State *L, lua_Object ltable) {
    PyObject *dict = PyDict_New();
    if (!dict) {
        lua_new_error(L, "failed to create key words arguments dict");
    }
    PyObject *key, *value;
    int index = lua_next(L, ltable, 0);

    while (index != 0) {
        key = lua_convert(L, 1);
        value = lua_convert(L, 2);

        PyDict_SetItem(dict, key, value);

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
        lua_error(L, "first arg need be table ex: kwargs({a=10})");
    }
    PyObject *dict = get_py_dict(L, ltable);
    py_object_wrap_lua(L, dict, 1);
}

/*get py object from wrap table (direct access) */
PyObject *get_pobject(lua_State *L, lua_Object ltable) {
    if (!lua_istable(L, ltable))
        lua_error(L, "wrap table not found!");

    // python object recover
    lua_pushobject(L, ltable);
    lua_pushstring(L, POBJECT);

    return lua_getuserdata(L, lua_rawgettable(L));
}

/*get py object from wrap table */
py_object *get_py_object(lua_State *L, lua_Object ltable) {
    if (!lua_istable(L, ltable))
        lua_error(L, "wrap table not found!");

    py_object *po = (py_object *) malloc(sizeof(py_object));
    if (po == NULL) {
        lua_error(L, "out of memory!");
    }
    // python object recover
    lua_pushobject(L, ltable);
    lua_pushstring(L, POBJECT);

    po->o = (PyObject *) lua_getuserdata(L, lua_rawgettable(L));

    lua_pushobject(L, ltable);
    lua_pushstring(L, ASINDX);

    po->asindx = (int) lua_getnumber(L, lua_rawgettable(L));

    // python object recover
    lua_pushobject(L, ltable);
    lua_pushstring(L, POMETA);

    po->meta = (py_object_meta *) lua_getuserdata(L, lua_rawgettable(L));
    return po;
}

/* get py object in stack */
py_object *get_py_object_stack(lua_State *L, int n) {
    return get_py_object(L, lua_getparam(L, n));
}

PyObject *lua_interpreter_object_convert(InterpreterObject *interpreter, int stackpos,
                                         lua_Object lobj) {
    PyObject *ret = NULL;
    if (lua_isnil(interpreter->L, lobj)) {
        Py_INCREF(Py_None);
        ret = Py_None;
    } else if (lua_isnumber(interpreter->L, lobj)) {
        double num = lua_getnumber(interpreter->L, lobj);
        if (rintf((float) num) == num) {  // is int?
            ret = PyInt_FromLong((long) num);
        } else {
            ret = PyFloat_FromDouble(num);
        }
    } else if (lua_isstring(interpreter->L, lobj)) {
        const char *s = lua_getstring(interpreter->L, lobj);
        int len = lua_strlen(interpreter->L, lobj);
        ret = PyString_FromStringAndSize(s, len);
        if (!ret) {
            ret = PyUnicode_FromStringAndSize(s, len);
        }
    } else if (is_wrapped_object(interpreter->L, lobj)) {
        ret = get_pobject(interpreter->L, lua_getparam(interpreter->L, stackpos));
    } else if (lua_isfunction(interpreter->L, lobj)) {
        if (stackpos && !lobj) {
            ret = LuaObject_New(interpreter, stackpos);
        } else {
            ret = LuaObject_PyNew(interpreter, lobj);
        }
    } else if (lua_istable(interpreter->L, lobj)) {
        lua_beginblock(interpreter->L);
        if (!PYTHON_EMBEDDED_MODE) { // Lua inside Python
            if (stackpos) {
                ret = LuaObject_New(interpreter, stackpos);
            } else {
                ret = LuaObject_PyNew(interpreter, lobj);
            }
        //  Python inside Lua
        } else if (is_indexed_array(interpreter->L, lobj)) {
            ret = _get_py_tuple(interpreter->L, lobj);
        } else {
            ret = get_py_dict(interpreter->L, lobj);
        }
        lua_endblock(interpreter->L);
    } else if (lua_isboolean(interpreter->L, lobj)) {
        if (lua_getboolean(interpreter->L, lobj)) {
            Py_INCREF(Py_True);
            ret = Py_True;
        } else {
            Py_INCREF(Py_False);
            ret = Py_False;
        }
    } else if (lua_isuserdata(interpreter->L, lobj)) {
        void *void_ptr = lua_getuserdata(interpreter->L, lobj); // userdata NULL ?
        if (void_ptr) {
            if (PYTHON_EMBEDDED_MODE) {
                ret = (PyObject *) void_ptr;
            } else {
                ret = LuaObject_PyNew(interpreter, lobj);
            }
        }  else {
            Py_INCREF(Py_None);
            ret = Py_None;
        }
    }
    return ret;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
PyObject *lua_stack_convert(lua_State *L, int stackpos, lua_Object lobj) {
    InterpreterObject *interpreter;
    interpreter = malloc(sizeof(InterpreterObject));
    if (!interpreter) lua_error(L, "out of memory!");
    interpreter->L = L;
    interpreter->malloc = true;
    interpreter->exit = false;
    interpreter->link = false;
    PyObject *ret = lua_interpreter_object_convert(interpreter, stackpos, lobj);
    if (!interpreter->link && interpreter->malloc) {
        free(interpreter); // Link with custom types
    }
    return ret;
}
#pragma clang diagnostic pop


PyObject *lua_interpreter_stack_convert(InterpreterObject *interpreter,
                                         int stackpos) {
    return lua_interpreter_object_convert(interpreter, stackpos,
                                          lua_getparam(interpreter->L, stackpos));
}

PyObject *lua_convert(lua_State *L, int stackpos) {
    return lua_stack_convert(L, stackpos, lua_getparam(L, stackpos));
}