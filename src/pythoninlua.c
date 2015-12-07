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
#include <stdbool.h>

#include "pythoninlua.h"

#ifndef lua_next
#include "lapi.h"
#endif

#include "luaconv.h"
#include "pyconv.h"
#include "utils.h"

// Variable to know in mode python was started (Inside Lua embedded).
bool PYTHON_EMBED_MODE = false;


static void py_object_call(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    PyObject *args = PyTuple_New(0);
    PyObject *kwargs = NULL;
    PyObject *value;

    int nargs = lua_gettop(L)-1;

    if (!PyCallable_Check(pobj->o)) {
        lua_error(L, "object is not callable");
    }
    lua_Object largs = lua_getparam(L, 2);
    lua_Object lkwargs = lua_getparam(L, 3);

    if (nargs == 1 && lua_isuserdata(L, largs)) {
        PyObject *pyobj = (PyObject *) lua_getuserdata(L, largs);
        if (PyTuple_Check(pyobj)) {
            args = pyobj;
        } else if (PyDict_Check(pyobj)) {
            kwargs = pyobj;
        } else {
            args = get_py_tuple(L, 1);
        }
    } else if (nargs == 2 && lua_isuserdata(L, largs) && lua_isuserdata(L, lkwargs)) {
        args = (PyObject *) lua_getuserdata(L, largs);   // is args and kwargs ?
        kwargs = (PyObject *) lua_getuserdata(L, lkwargs);

        // check the order (), {}
        if (PyTuple_Check(kwargs)) luaL_argerror(L, 1, "object tuple expected args(1,...)");
        if (PyDict_Check(args)) luaL_argerror(L, 2, "object dict expected kwargs{a=1,...}");

    } else if (nargs > 0) {
        args = get_py_tuple(L, 1); // arbitrary args fn(1,2,'a')
    }
    value = PyObject_Call(pobj->o, args, kwargs); // fn(*args, **kwargs)
    if (value) {
        py_convert(L, value);
        Py_DECREF(value);
    } else {
        char *name = get_pyobject_str(pobj->o, "...");
        char *error = "call python function \"%s\"";
        char buff[calc_buff_size(2, error, name)];
        sprintf(buff, error, name);
        lua_new_error(L, buff);
    }
    free(pobj);
}

static int _p_object_newindex_set(lua_State *L, py_object *obj, int keyn, int valuen) {
    PyObject *value;
    PyObject *key = lua_convert(L, keyn);
    if (!key) luaL_argerror(L, 1, "failed to convert key");

    lua_Object lobj = lua_getparam(L, valuen);

    if (!lua_isnil(L, lobj)) {
        value = lua_convert(L, valuen);
        if (!value) {
            Py_DECREF(key);
            luaL_argerror(L, 1, "failed to convert value");
        }
        // setitem (obj[0] = 1) if int else setattr(obj.val = 1)
        if (obj->asindx) {
            if (PyObject_SetItem(obj->o, key, value) == -1) {
                lua_new_error(L, "failed to set item");
            }
        } else if (PyObject_SetAttr(obj->o, key, value) == -1) {
            lua_new_error(L, "failed to set item");
        }
        Py_DECREF(value);
    } else {
        if (PyObject_DelItem(obj->o, key) == -1) {
            lua_new_error(L, "failed to delete item");
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

static int _p_object_index_get(lua_State *L, py_object *pobj, int keyn) {
    PyObject *key = lua_convert(L, keyn);
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
        char *error = "%s \"%s\" not found";
        char *name = pobj->asindx ? "index" : "attribute";
        char *skey = get_pyobject_str(key, "...");
        char buff[calc_buff_size(3, error, name, skey)];
        sprintf(buff, error, name, skey);
        lua_new_error(L, buff);
    }
    return ret;
}

static void py_object_index(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    _p_object_index_get(L, pobj, 2);
    free(pobj);
}

static void py_object_gc(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    Py_CLEAR(pobj->o);
    free(pobj);
}

static void py_object_tostring(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
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
        lua_new_error(L, "run custom code");
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
    Py_DECREF(pobj->o);
    py_object_wrap_lua(L, pobj->o, 1);
    free(pobj);
}

static void py_asattr(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
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
        lua_new_error(L, "can't get globals");
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
        lua_new_error(L, "failed to get builtins");
    }
    py_object_wrap_lua(L, builtins, 1);
}

static void py_import(lua_State *L) {
    const char *name = luaL_check_string(L, 1);
    PyObject *module;

    if (!name) luaL_argerror(L, 1, "module name expected");

    module = PyImport_ImportModule((char *) name);

    if (!module) {
        char *error = "failed importing \"%s\"";
        char buff[calc_buff_size(2, error, name)];
        sprintf(buff, error, name);
        lua_new_error(L, buff);
    }

    py_object_wrap_lua(L, module, 0);
    Py_DECREF(module);
}

static void python_system_init(lua_State *L);

/** Ends the Python interpreter, freeing resources*/
static void python_system_exit(lua_State *L) {
    if (Py_IsInitialized())
        Py_Finalize();
}

/* Indicates if Python interpreter was embedded in the Lua */
static void python_is_embedded(lua_State *L) {
    if (PYTHON_EMBED_MODE) {
        lua_pushnumber(L, 1);
    } else {
        lua_pushnil(L);
    }
}

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
    {"system_exit", python_system_exit},
    {"args"       , py_args},
    {"kwargs"     , py_kwargs},
    {"is_embedded", python_is_embedded},
    {"raw"        , lua_raw},
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
    PYTHON_EMBED_MODE = false;  // If Python is inside Lua

    lua_Object python = lua_createtable(L);

    lua_pushcfunction(L, py_args);
    lua_setglobal(L, "pyargs");

    lua_pushcfunction(L, py_kwargs);
    lua_setglobal(L, "pykwargs");

    lua_pushobject(L, python);
    lua_setglobal(L, "python");  // api python

    int index = 0;
    while (py_lib[index].name) {
        set_table_fn(L, python, py_lib[index].name, py_lib[index].func);
        index++;
    }
    set_table_userdata(L, python, "True", Py_True);
    Py_INCREF(Py_True);
    set_table_userdata(L, python, "False", Py_False);
    Py_INCREF(Py_False);

    // base python object
    lua_Object ltable = lua_createtable(L);
    set_table_object(L, python, POBJECT, ltable);
    set_table_userdata(L, ltable, "base", Py_True);
    Py_INCREF(Py_True);

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
        PYTHON_EMBED_MODE = true; // If Python is inside Lua
        if (PyType_Ready(&LuaObject_Type) == 0) {
            Py_INCREF(&LuaObject_Type);
        } else {
            lua_error(L, "failure initializing lua object type");
        }
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
