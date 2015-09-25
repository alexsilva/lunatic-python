/*

 Lunatic Python
 --------------

 Copyright (c) 2002-2005  Gustavo Niemeyer <gustavo@niemeyer.net>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
#include <Python.h>

#include <lua.h>
#include <lauxlib.h>

#include "pythoninlua.h"
#ifndef lua_next
#include "lapi.h"
#endif


// ----------------------------------------
static int lua_gettop(lua_State *L) {
    return L->Cstack.num;
}

static int lua_isboolean(lua_State *L, lua_Object obj) {
    return lua_isuserdata(L, obj) && PyBool_Check((PyObject *) lua_getuserdata(L, obj));
}

static int lua_getboolean(lua_State *L, lua_Object obj) {
    return PyObject_IsTrue((PyObject *) lua_getuserdata(L, obj));
}

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

/*Base table object*/
static int get_base_tag(lua_State *L) {
    lua_Object python = lua_getglobal(L, "python");
    lua_pushobject(L, python);
    lua_pushstring(L, POBJECT);
    return lua_tag(L, lua_gettable(L));
}

/*checks if a table contains only numbers*/
static int is_indexed_array(lua_State *L, lua_Object lobj) {
    int index = lua_next(L, lobj, 1);
    lua_Object key;
    while (index != 0) {
        key = lua_getparam(L, 1);
        if (!lua_isnumber(L, key))
            return 0;
        index = lua_next(L, lobj, index);
    }
    return 1;
}

static PyObject *lua_as_py_object(lua_State *L, int n) {
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
                lua_error(L, "param not supported");
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
    }
    return ret;
}
// ----------------------------------------

static int py_object_wrap_lua(lua_State *L, PyObject *pobj, int asindx) {
    Py_INCREF(pobj);
    Py_INCREF(pobj);

    lua_Object ltable = lua_createtable(L);

    set_table_userdata(L, ltable, POBJECT, pobj);
    set_table_number(L, ltable, ASINDX, asindx);

    // register all tag methods
    int tag = get_base_tag(L);
    lua_pushobject(L, ltable);
    lua_settag(L, tag);

    // returning table
    lua_pushobject(L, ltable);
    return 1;
}

static int py_convert(lua_State *, PyObject *);

/* python object presentation */
static char *get_pyobject_repr(lua_State *L, PyObject *pyobject) {
    char *repr = "...";
    char *name = "__name__"; // get real name!
    if (PyObject_HasAttrString(pyobject, name)) {
        pyobject = PyObject_GetAttrString(pyobject, name);
    }
    PyObject *str = PyObject_Str(pyobject);
    if (str) {
        repr = PyString_AsString(pyobject);
    }
    return repr;
}

/* python string bytes */
static char *get_pyobject_as_string(lua_State *L, PyObject *o) {
    char *s = PyString_AsString(o);
    if (!s) {
        PyErr_Print();
        lua_error(L, "converting python string");
    }
    return s;
}

/* python string unicode */
static char *get_pyobject_as_utf8string(lua_State *L, PyObject *o) {
    o = PyUnicode_AsUTF8String(o);
    if (!o) {
        PyErr_Print();
        lua_error(L, "converting unicode string");
    }
    return get_pyobject_as_string(L, o);
}

static int py_convert(lua_State *L, PyObject *o) {
    int ret = 0;
    if (o == Py_None || o == Py_False) {
        lua_pushnil(L);
        ret = 1;
    } else if (o == Py_True) {
        lua_pushnumber(L, 1);
        ret = 1;
#if PY_MAJOR_VERSION >= 3
    } else if (PyUnicode_Check(o)) {
        Py_ssize_t len;
        char *s = PyUnicode_AsUTF8AndSize(o, &len);
#else
    } else if (PyString_Check(o)) {
        lua_pushstring(L, get_pyobject_as_string(L, o));
        ret = 1;
    } else if (PyUnicode_Check(o)) {
        char *s = get_pyobject_as_utf8string(L, o);
#endif
        lua_pushstring(L, s);
        ret = 1;
#if PY_MAJOR_VERSION < 3
    } else if (PyInt_Check(o)) {
        lua_pushnumber(L, PyInt_AsLong(o));
        ret = 1;
#endif
    } else if (PyLong_Check(o)) {
        lua_pushnumber(L, PyLong_AsLong(o));
        ret = 1;
    } else if (PyFloat_Check(o)) {
        lua_pushnumber(L, PyFloat_AsDouble(o));
        ret = 1;
    } else {
        int asindx = 0;
        if (PyList_Check(o) || PyTuple_Check(o) || PyDict_Check(o))
            asindx = 1;
        ret = py_object_wrap_lua(L, o, asindx);
    }
    return ret;
}

static void py_object_call(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    PyObject *args;
    PyObject *value;

    int nargs = lua_gettop(L)-1;
    int i;

    if (!pobj) {
        luaL_argerror(L, 1, "not a python object");
    }
    if (!PyCallable_Check(pobj->o)) {
        lua_error(L, "object is not callable");
    }

    args = PyTuple_New(nargs);
    if (!args) {
        PyErr_Print();
        lua_error(L, "failed to create arguments tuple");
    }

    for (i = 0; i != nargs; i++) {
        PyObject *arg = lua_as_py_object(L, i + 2);
        if (!arg) {
            Py_DECREF(args);
            char *error = "failed to convert argument #%d";
            char buff[strlen(error) + 10];
            sprintf(buff, error, i + 1);
            lua_error(L, &buff[0]);
        }
        PyTuple_SetItem(args, i, arg);
    }

    value = PyObject_CallObject(pobj->o, args);
    if (value) {
        py_convert(L, value);
        Py_DECREF(value);
    } else {
        PyErr_Print();
        char *name = get_pyobject_repr(L, pobj->o);
        char *error = "calling python function \"%s\"";
        char buff[strlen(error) + strlen(name) + strlen(name) + 1];
        sprintf(buff, error, name);
        lua_error(L, &buff[0]);
    }
    free(pobj);
}

static int _p_object_newindex_set(lua_State *L, py_object *obj, int keyn, int valuen) {
    PyObject *value;
    PyObject *key = lua_as_py_object(L, keyn);
    if (!key) luaL_argerror(L, 1, "failed to convert key");

    lua_Object lobj = lua_getparam(L, valuen);

    if (!lua_isnil(L, lobj)) {
        value = lua_as_py_object(L, valuen);
        if (!value) {
            Py_DECREF(key);
            luaL_argerror(L, 1, "failed to convert value");
        }
        // setitem (obj[0] = 1) if int else setattr(obj.val = 1)
        if (obj->asindx) {
            if (PyObject_SetItem(obj->o, key, value) == -1) {
                PyErr_Print();
                lua_error(L, "failed to set item");
            }
        } else if (PyObject_SetAttr(obj->o, key, value) == -1) {
            PyErr_Print();
            lua_error(L, "failed to set item");
        }
        Py_DECREF(value);
    } else {
        if (PyObject_DelItem(obj->o, key) == -1) {
            PyErr_Print();
            lua_error(L, "failed to delete item");
        }
    }
    Py_DECREF(key);
    return 0;
}

static void py_object_newindex_set(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    if (lua_gettop(L) < 2) {
        lua_error(L, "invalid arguments");
    }
    _p_object_newindex_set(L, pobj, 2, 3);
    free(pobj);
}

static void py_object_newindex(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    const char *attr;
    PyObject *value;

    if (!pobj) {
        luaL_argerror(L, 1, "not a python object");
    }

    if (pobj->asindx) {
        _p_object_newindex_set(L, pobj, 2, 3);
        return;
    }
    attr = luaL_check_string(L, 2);
    if (!attr) {
        luaL_argerror(L, 2, "string needed");
    }

    value = lua_as_py_object(L, 3);
    if (!value) {
        luaL_argerror(L, 1, "failed to convert value");
    }

    if (PyObject_SetAttrString(pobj->o, (char *) attr, value) == -1) {
        Py_DECREF(value);
        PyErr_Print();
        lua_error(L, "failed to set value");
    }

    Py_DECREF(value);
    free(pobj);
}

static int _p_object_index_get(lua_State *L, py_object *pobj, int keyn) {
    PyObject *key = lua_as_py_object(L, keyn);
    PyObject *item;
    int ret = 0;

    if (!key) luaL_argerror(L, 1, "failed to convert key");

    if (pobj->asindx) {
        item = PyObject_GetItem(pobj->o, key);
    } else {
        item = PyObject_GetAttr(pobj->o, key);
    }
    Py_DECREF(key);
    if (item) {
        ret = py_convert(L, item);
        Py_DECREF(item);
    } else {
        PyErr_Clear();
        char *keystr = get_pyobject_repr(L, key);
        char *error = "%s \"%s\" not found";
        char *name = pobj->asindx ? "index or key" : "attribute";
        char buff[strlen(error) + strlen(name) + strlen(keystr) + 1];
        sprintf(buff, error, name, keystr);
        lua_error(L, &buff[0]);
    }
    return ret;
}

static void py_object_index(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    if (!pobj) luaL_argerror(L, 1, "not a python object");
    _p_object_index_get(L, pobj, 2);
    free(pobj);
}

static void py_object_gc(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    if (pobj) {
        Py_CLEAR(pobj->o);
    }
    free(pobj);
}

static void py_object_tostring(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    if (pobj) {
        PyObject *repr = PyObject_Str(pobj->o);
        if (!repr) {
            char buf[256];
            snprintf(buf, 256, "python object: %p", pobj->o);
            lua_pushstring(L, buf);
            PyErr_Clear();
        } else {
            py_convert(L, repr);
            Py_DECREF(repr);
        }
    }
    free(pobj);
}

static int py_run(lua_State *L, int eval) {
    const char *s;
    char *buffer = NULL;
    PyObject *m, *d, *o;
    int ret = 0;
    size_t len;

    s = luaL_check_string(L, 1);
    if (!s)
        return 0;

    if (!eval) {
        len = strlen(s) + 1;
        buffer = (char *) malloc(len + 1);
        if (!buffer) {
            lua_error(L, "Failed allocating buffer for execution");
        }
        strcpy(buffer, s);
        buffer[len - 1] = '\n';
        buffer[len] = '\0';
        s = buffer;
    }

    m = PyImport_AddModule("__main__");
    if (!m) {
        free(buffer);
        lua_error(L, "Can't get __main__ module");
    }
    d = PyModule_GetDict(m);

    o = PyRun_StringFlags(s, eval ? Py_eval_input : Py_single_input,
                          d, d, NULL);

    free(buffer);

    if (!o) {
        PyErr_Print();
        return 0;
    }

    if (py_convert(L, o))
        ret = 1;

    Py_DECREF(o);

#if PY_MAJOR_VERSION < 3
    if (Py_FlushLine())
#endif
        PyErr_Clear();

    return ret;
}

static void py_execute(lua_State *L) {
    py_run(L, 0);
}

static void py_eval(lua_State *L) {
    py_run(L, 1);
}

static void py_asindx(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    if (!pobj) {
        luaL_argerror(L, 1, "not a python object");
    }
    Py_DECREF(pobj->o);
    py_object_wrap_lua(L, pobj->o, 1);
    free(pobj);
}

static void py_asattr(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    if (!pobj) {
        luaL_argerror(L, 1, "not a python object");
    }
    Py_DECREF(pobj->o);
    py_object_wrap_lua(L, pobj->o, 0);
    free(pobj);
}

static void py_globals(lua_State *L) {
    PyObject *globals;
    if (lua_gettop(L) != 0) {
        lua_error(L, "invalid arguments");
    }
    globals = PyEval_GetGlobals();
    if (!globals) {
        PyObject *module = PyImport_AddModule("__main__");
        if (!module) {
            lua_error(L, "Can't get __main__ module");
        }
        globals = PyModule_GetDict(module);
    }
    if (!globals) {
        PyErr_Print();
        lua_error(L, "can't get globals");
    }
    py_object_wrap_lua(L, globals, 1);
}

static void py_locals(lua_State *L) {
    PyObject *locals;

    if (lua_gettop(L) != 0) {
        lua_error(L, "invalid arguments");
    }
    locals = PyEval_GetLocals();
    if (!locals) {
        py_globals(L);
        return;
    }
    py_object_wrap_lua(L, locals, 1);
}

static void py_builtins(lua_State *L) {
    PyObject *builtins;

    if (lua_gettop(L) != 0) {
        lua_error(L, "invalid arguments");
    }

    builtins = PyEval_GetBuiltins();
    if (!builtins) {
        PyErr_Print();
        lua_error(L, "failed to get builtins");
    }
    py_object_wrap_lua(L, builtins, 1);
}

static void py_import(lua_State *L) {
    const char *name = luaL_check_string(L, 1);
    PyObject *module;

    if (!name) luaL_argerror(L, 1, "module name expected");

    module = PyImport_ImportModule((char *) name);

    if (!module) {
        PyErr_Print();
        char *error = "failed importing \"%s\"";
        char buff[strlen(error) + strlen(name) + 1];
        sprintf(buff, error, name);
        lua_error(L, &buff[0]);
    }

    py_object_wrap_lua(L, module, 0);
    Py_DECREF(module);
}

static void python_system_init(lua_State *L);

static struct luaL_reg py_lib[] = {
        {"execute",     py_execute},
        {"eval",        py_eval},
        {"asindex",     py_asindx},
        {"asattr",      py_asattr},
        {"str",         py_object_tostring},
        {"locals",      py_locals},
        {"globals",     py_globals},
        {"builtins",    py_builtins},
        {"import",      py_import},
        {"system_init", python_system_init},
        {NULL, NULL}
};

static struct luaL_reg lua_tag_methods[] = {
        {"function", py_object_call},
        {"index",    py_object_index},
        {"settable", py_object_newindex_set},
        {"gc",       py_object_gc},
        {NULL, NULL}
};


/* Register module */
LUA_API int luaopen_python(lua_State *L) {
    lua_Object python = lua_createtable(L);

    lua_pushobject(L, python);
    lua_setglobal(L, "python");  // api python

    int index = 0;
    while (py_lib[index].name) {
        set_table_fn(L, python, py_lib[index].name, py_lib[index].func);
        index++;
    }

    set_table_userdata(L, python, "True", Py_True);
    set_table_userdata(L, python, "False", Py_False);

    // base python object
    lua_Object ltable = lua_createtable(L);
    set_table_object(L, python, POBJECT, ltable);

    // register all tag methods
    int ntag = lua_newtag(L);
    index = 0;
    while (lua_tag_methods[index].name) {
        lua_pushcfunction(L, lua_tag_methods[index].func);
        lua_settagmethod(L, ntag, lua_tag_methods[index].name);
        index++;
    }

    // set tag
    lua_pushobject(L, ltable);
    lua_settag(L, ntag);

    // try startup system
    // python_system_init(L);
    return 0;
}

/* Initialize Python interpreter */
static void python_system_init(lua_State *L) {
    char *python_home = luaL_check_string(L, 1);

    if (!Py_IsInitialized()) {
        PyObject *luam, *mainm, *maind;
#if PY_MAJOR_VERSION >= 3
        wchar_t *argv[] = {L"<lua>", 0};
#else
        char *argv[] = {"<lua>", 0};
#endif
        Py_SetProgramName(argv[0]);
        Py_SetPythonHome(python_home);
        Py_Initialize();
        PySys_SetArgv(1, argv);
        /* Import 'lua' automatically. */
        luam = PyImport_ImportModule("lua");
        if (!luam) {
            lua_error(L, "Can't import lua module");
        } else {
            mainm = PyImport_AddModule("__main__");
            if (!mainm) {
                lua_error(L, "Can't get __main__ module");
            } else {
                maind = PyModule_GetDict(mainm);
                PyDict_SetItemString(maind, "lua", luam);
                Py_DECREF(luam);
            }
        }
    }
}
