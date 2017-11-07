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

#include "luaconv.h"
#include "pyconv.h"
#include "utils.h"
#include "constants.h"
#include "auxiliary.h"
#include "stack.h"


static void py_object_call(lua_State *L) {
    py_object *pobj = get_py_object(L, lua_getparam(L, 1));
    if (!PyCallable_Check(pobj->object)) {
        const char *name = pobj->object->ob_type->tp_name;
        char *format = "object \"%s\" is not callable";
        const char *str = name ? name : "?";
        char buff[buffsize_calc(2, format, str)];
        sprintf(buff, format, str);
        lua_new_error(L, &buff[0]);
    }
    PyObject *args = NULL;
    PyObject *kwargs = NULL;
    PyObject *value;

    int nargs = lua_gettop(L)-1;

    lua_Object largs = lua_getparam(L, 2);
    lua_Object lkwargs = lua_getparam(L, nargs > 1 ? 3 : 2);

    bool isargs = ispyargs(L, largs), iskwargs = ispykwargs(L, lkwargs);

    if (nargs == 1 && (isargs || iskwargs)) {
        PyObject *obj = get_pobject(L, largs);
        if (PyTuple_Check(obj)) {
            args = obj;
        } else if (PyDict_Check(obj)) {
            args = PyTuple_New(0);
            if (!args) lua_new_error(L, "#1 failed to create arguments tuple");
            kwargs = obj;
        } else {
            lua_error(L, "reference pyargs or pykwargs invalid!");
        }
    } else if (nargs == 2 && isargs && iskwargs) {
        args   = get_pobject(L, largs);
        kwargs = get_pobject(L, lkwargs); // is args and kwargs ?
        // check the order (), {}
        if (PyTuple_Check(kwargs)) luaL_argerror(L, 1, "object tuple expected args(1,...)");
        if (PyDict_Check(args)) luaL_argerror(L, 2, "object dict expected kwargs{a=1,...}");

    } else if (nargs > 0) {
        python_setnumber(L, PY_LUA_TABLE_CONVERT, 1);
        args = get_py_tuple(L, 1); // arbitrary args fn(1,2,'a')
        python_setnumber(L, PY_LUA_TABLE_CONVERT, 0);
        isargs = false;
    } else {
        args = PyTuple_New(0);
        if (!args) lua_new_error(L, "#3 failed to create arguments tuple");
    }
    value = PyObject_Call(pobj->object, args, kwargs); // fn(*args, **kwargs)
    if (!isargs) Py_DECREF(args);
    if (!iskwargs && kwargs)
        Py_DECREF(kwargs);
    if (value) {
        if (py_convert(L, value) == CONVERTED) {
            Py_DECREF(value);
        }
    } else {
        lua_raise_error(L, "call function python \"%s\"", pobj->object);
    }
}

static int set_py_object_index(lua_State *L, py_object *pobj, int keyn, int valuen) {
    PyObject *key = lua_stack_convert(L, keyn);
    if (!key) luaL_argerror(L, 1, "failed to convert key");
    lua_Object lval = lua_getparam(L, valuen);
    if (!lua_isnil(L, lval)) {
        PyObject *value = lua_object_convert(L, lval);
        if (!value) {
            Py_DECREF(key); luaL_argerror(L, 1, "failed to convert value");
        }
        // setitem (obj[0] = 1) if int else setattr(obj.val = 1)
        if (pobj->asindx) {
            if (PyObject_SetItem(pobj->object, key, value) == -1) {
                Py_DECREF(key); Py_DECREF(value);
                lua_new_error(L, "failed to set item");
            }
        } else if (PyObject_SetAttr(pobj->object, key, value) == -1) {
            Py_DECREF(key); Py_DECREF(value);
            lua_new_error(L, "failed to set attribute");
        }
        Py_DECREF(value);
    } else {
        if (PyObject_DelItem(pobj->object, key) == -1) {
            Py_DECREF(key); lua_new_error(L, "failed to delete item");
        }
    }
    Py_DECREF(key);
    return 0;
}

static void py_object_index_set(lua_State *L) {
    py_object *pobj = get_py_object(L, lua_getparam(L, 1));
    if (lua_gettop(L) < 2) {
        lua_error(L, "invalid arguments");
    }
    set_py_object_index(L, pobj, 2, 3);
}

static int get_py_object_index(lua_State *L, py_object *pobj, int keyn) {
    PyObject *key = lua_stack_convert(L, keyn);
    Conversion ret = UNCHANGED;
    PyObject *item;
    if (!key) luaL_argerror(L, 1, "failed to convert key");
    if (pobj->asindx) {
        item = PyObject_GetItem(pobj->object, key);
    } else {
        item = PyObject_GetAttr(pobj->object, key);
    }
    if (item) {
        if ((ret = py_convert(L, item)) == CONVERTED) {
            Py_DECREF(item);
        }
    } else {
        char *error = "%s \"%s\" not found";
        char *name = pobj->asindx ? "index" : "attribute";
        char *mkey = get_pyobject_str(key);
        char *skey = mkey ? mkey : "?";
        char buff[buffsize_calc(3, error, name, skey)];
        sprintf(buff, error, name, skey);
        free(mkey); // free pointer!
        Py_DECREF(key);
        lua_new_error(L, buff);
    }
    Py_DECREF(key);
    return ret;
}

static void py_object_index_get(lua_State *L) {
    get_py_object_index(L, get_py_object(L, lua_getparam(L, 1)), 2);
}

static void py_object_gc(lua_State *L) {
    py_object *pobj = lua_getuserdata(L, lua_getparam(L, 1));
    if (pobj) {
        if (Py_IsInitialized())
            Py_XDECREF(pobj->object);
        free(pobj);
    }
}

// Represents a python object.
static void py_object_repr(lua_State *L) {
    PyObject *obj = get_pobject(L, lua_getparam(L, 1));
    PyObject *repr = PyObject_Str(obj);
    if (!repr) {
        char buf[256];
        snprintf(buf, 256, "python object: %p", obj);
        lua_pushstring(L, buf);
        PyErr_Clear();
    } else {
        py_convert(L, repr);
        Py_DECREF(repr);
    }
}

static int py_run(lua_State *L, int eval) {
    const char *s;
    char *buffer = NULL;
    PyObject *m, *d, *o;
    Conversion ret;
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
    if ((ret = py_convert(L, o)) == CONVERTED) {
        Py_DECREF(o);
    }
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

/**
 * Change the mode of access to object references to indexes
 * Ex:
 * local sys = python.import(sys)
 * sys.path[0]  # '0' as index (default)
**/
static void py_asindx(lua_State *L) {
    py_object *pobj = get_py_object(L, lua_getparam(L, 1));
    Py_INCREF(pobj->object); // new ref
    push_pyobject_container(L, pobj->object, true);
}

/**
 * Change the mode of access to object references to attributes
 * Ex:
 * local sys = python.import(sys)
 * python.asattr(sys.path).pop(0) # 'pop' as attribute
**/
static void py_asattr(lua_State *L) {
    py_object *pobj = get_py_object(L, lua_getparam(L, 1));
    Py_INCREF(pobj->object); // new ref
    push_pyobject_container(L, pobj->object, false);
}

/* Enables list and tuple as arguments */
static void py_asargs(lua_State *L) {
    py_object *pobj = get_py_object(L, lua_getparam(L, 1));
    if (PyObject_IsListInstance(pobj->object)) {
        PyObject *obj = PyList_AsTuple(pobj->object);
        pobj = py_object_container(L, obj, true);
        pobj->isargs = true;
        lua_pushusertag(L, pobj, python_api_tag(L));
    } else if (PyObject_IsTupleInstance(pobj->object)) {
        Py_INCREF(pobj->object);
        pobj = py_object_container(L, pobj->object, true);
        pobj->isargs = true;
        lua_pushusertag(L, pobj, python_api_tag(L));
    } else {
        luaL_argerror(L, 1, "tuple or list expected");
    }
}

/* Enables dictionaries as keyword arguments */
static void py_askwargs(lua_State *L) {
    py_object *pobj = get_py_object(L, lua_getparam(L, 1));
    if (PyObject_IsDictInstance(pobj->object)) {
        Py_INCREF(pobj->object);
        pobj = py_object_container(L, pobj->object, true);
        pobj->iskwargs = true;
        lua_pushusertag(L, pobj, python_api_tag(L));
    } else {
        luaL_argerror(L, 1, "dict expected");
    }
}

/**
 * Returns the globals dictionary
**/
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
    Py_INCREF(globals);
    push_pyobject_container(L, globals, 1);
}

/**
 * Returns the locals dictionary
**/
static void py_locals(lua_State *L) {
    PyObject *locals;
    if (lua_gettop(L) != 0) {
        lua_error(L, "invalid arguments");
    }
    locals = PyEval_GetLocals();
    if (!locals) {
        py_globals(L);
    } else { //Py_INCREF(locals);
        push_pyobject_container(L, locals, 1);
    }
}

/**
 * Returns the builtins dictionary
**/
static void py_builtins(lua_State *L) {
    PyObject *builtins;
    if (lua_gettop(L) != 0) {
        lua_error(L, "invalid arguments");
    }
    builtins = PyEval_GetBuiltins();
    if (!builtins) {
        lua_new_error(L, "failed to get builtins");
    }
    Py_INCREF(builtins);
    push_pyobject_container(L, builtins, 1);
}

/**
 * Import a new module and returns its reference
 * Lua stack #1: module name (str)
**/
static void py_import(lua_State *L) {
    const char *name = luaL_check_string(L, 1);
    PyObject *module;
    if (!name) {
        luaL_argerror(L, 1, "module name expected");
    }
    module = PyImport_ImportModule((char *) name);
    if (!module) {
        char *error = "failed importing \"%s\"";
        char buff[buffsize_calc(2, error, name)];
        sprintf(buff, error, name);
        lua_new_error(L, buff);
    }
    push_pyobject_container(L, module, 0);
}

/* return version of the python extension */
static void py_get_version(lua_State *L) {
    lua_pushstring(L, PY_EXT_VERSION);
}

/* Turn off the conversion of object */
static void py_byref(lua_State *L) {
    int stacked = is_byref(L);
    if (!stacked) set_byref(L, 1);
    py_object_index_get(L);
    if (!stacked) set_byref(L, 0);
}

/* Turn off the conversion of object */
static void py_byrefc(lua_State *L) {
    int stacked = is_byref(L);
    if (!stacked) set_byref(L, 1);
    py_object_call(L);
    if (!stacked) set_byref(L, 0);
}

/* Returns the number of registration of the events tag */
static void py_get_tag(lua_State *L) {
    lua_pushnumber(L, python_getnumber(L, PY_API_TAG));
}

/* allows the setting error control string in unicode string conversion */
static void _set_unicode_encoding_errorhandler(lua_State *L, int stackpos) {
    lua_Object lobj = lua_getparam(L, stackpos);
    if (lua_isstring(L, lobj)) {
        char *handler = lua_getstring(L, lobj);
        lobj = lua_getparam(L, stackpos + 1);
        if (lobj == LUA_NOOBJECT ? 1 : (int) lua_getnumber(L, lobj)) { // default true
            char *handlers[] = {"strict", "replace", "ignore"};
            bool found = false;
            for (int index = 0; index < 3; index++) {
                if (strcmp(handlers[index], handler) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                lua_new_error(L, "error handler to unicode string invalid. " \
                             "choices are: \"strict\", \"replace\", \"ignore\"");
            }
        }
        python_setstring(L, PY_UNICODE_ENCODING_ERRORHANDLER, handler);
    }
}

/* function that allows changing the default encoding */
static void py_set_unicode_encoding(lua_State *L) {
    python_setstring(L, PY_UNICODE_ENCODING, luaL_check_string(L, 1));
    _set_unicode_encoding_errorhandler(L, 2);
}

/* Allows the setting error control string in unicode string conversion */
static void py_set_unicode_encoding_errorhandler(lua_State *L) {
     _set_unicode_encoding_errorhandler(L, 1); 
}

/* Returns the encoding used in the string conversion */
static void py_get_unicode_encoding(lua_State *L) {
     lua_pushstring(L, python_getstring(L, PY_UNICODE_ENCODING)); 
}

/* Returns the string of errors controller */
static void py_get_unicode_encoding_errorhandler(lua_State *L) {
     lua_pushstring(L, python_getstring(L, PY_UNICODE_ENCODING_ERRORHANDLER)); 
}

/* Convert a Lua table into a Python dictionary */
static void table2dict(lua_State *L) {
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 1);
    push_pyobject_container(L, get_py_dict(L, luaL_tablearg(L, 1)), true);
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 0);
}

/* Convert a Lua table to a python tuple */
static void table2tuple(lua_State *L) {
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 1);
    push_pyobject_container(L, ltable_convert_tuple(L, luaL_tablearg(L, 1)), true);
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 0);
}

/* Convert a Lua table to a python list */
static void table2list(lua_State *L) {
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 1);
    push_pyobject_container(L, ltable2list(L, luaL_tablearg(L, 1)), true);
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 0);
}

/* Split lists and tuples slices o[start:end] */
static void pyobject_slice(lua_State *L) {
    lua_Object lobj = lua_getparam(L, 1);
    if (is_object_container(L, lobj)) {
        int start = luaL_check_int(L, 2);
        int end = luaL_check_int(L, 3);
        PyObject *object = get_pobject(L, lobj);
        PyObject *obj = PySequence_GetSlice(object, start, end);
        if (!obj) {
            const char *name = object->ob_type->tp_name;
            char *format = "object \"%s\" does not support slices";
            const char *str = name ? name : "?";
            char buff[buffsize_calc(2, format, str)];
            sprintf(buff, format, str);
            lua_new_error(L, &buff[0]);
        }
        push_pyobject_container(L, obj, 1);
    } else {
        lua_error(L, "#1 is not a container for python object!");
    }
}

static void py_state_restore(lua_State *L) {
    python_setnumber(L, PY_OBJECT_BY_REFERENCE, 0);
    python_setnumber(L, PY_LUA_TABLE_CONVERT, 0);
}

static void python_system_init(lua_State *L);

/** Ends the Python interpreter, freeing resources*/
static void python_system_exit(lua_State *L) {
    if (Py_IsInitialized() && python_getnumber(L, PY_API_IS_EMBEDDED))
        Py_Finalize();
}

/* Indicates if Python interpreter was embedded in the Lua */
static void python_is_embedded(lua_State *L) {
    if (python_getnumber(L, PY_API_IS_EMBEDDED)) {
        lua_pushnumber(L, 1);
    } else {
        lua_pushnil(L);
    }
}


static struct luaL_reg py_lib[] = {
    {"execute",                           py_execute}, // run arbitrary expressions in the interpreter.
    {"eval",                              py_eval},  // assesses the value of a variable and returns its reference.
    {"asindex",                           py_asindx}, // change the mode of access to attributes of an object for indexes.
    {"asattr",                            py_asattr}, // changes the way to access the attributes of an object for attributes.
    {"repr",                              py_object_repr}, // represents the object as a string (str(o)).
    {"locals",                            py_locals}, // returns the local scope variables dictionary.
    {"globals",                           py_globals}, // returns the global scope variables dictionary.
    {"builtins",                          py_builtins}, // returns the dictionary embedded objects.
    {"import",                            py_import}, // importing a module by its name (import("os")).
    {"system_init",                       python_system_init}, // initializes the interpreter in the location.
    {"system_exit",                       python_system_exit}, // terminates the interpreter (when embedded).
    {"args",                              py_args},
    {"kwargs",                            py_kwargs},
    {"args_array",                        py_args_array},
    {"is_embedded",                       python_is_embedded}, // report of the python interpreter was embedded in the Lua
    {"get_version",                       py_get_version}, // return release of the extension.
    {"set_unicode_encoding",              py_set_unicode_encoding},
    {"get_unicode_encoding",              py_get_unicode_encoding},
    {"get_unicode_encoding_errorhandler", py_get_unicode_encoding_errorhandler},
    {"set_unicode_encoding_errorhandler", py_set_unicode_encoding_errorhandler},
    {"byref",                             py_byref}, // returns the result reference (no conversion).
    {"byrefc",                            py_byrefc}, // returns the result reference (no conversion).
    {"tag",                               py_get_tag}, // returns the container tag objects python.
    {"dict",                              table2dict}, // returns a converted table to dictionary.
    {"tuple",                             table2tuple}, // returns a converted table to tuple.
    {"list",                              table2list}, // returns a converted table to list.
    {"table",                             pyobj2table}, // convert dict, list or tuple for a table.
    {"raw",                               pyobj2table}, // convert dict, list or tuple for a table.
    {"slice",                             pyobject_slice},
    {"asargs",                            py_asargs},
    {"askwargs",                          py_askwargs},
    {"readfile",                          py_readfile},
    {"restore",                           py_state_restore},
    {NULL, NULL}
};

static struct luaL_reg lua_tag_methods[] = {
    {"function", py_object_call},
    {"gettable", py_object_index_get},
    {"settable", py_object_index_set},
    {"gc",       py_object_gc},
    {NULL, NULL}
};

/* clean resources */
static void python_gc_function(lua_State *L) {
}

/* Register module */
LUA_API int luaopen_python(lua_State *L) {
    lua_Object python = lua_createtable(L);

    int pyntag = lua_newtag(L);
    lua_pushcfunction(L, python_gc_function);
    lua_settagmethod(L, pyntag, "gc");
    lua_pushobject(L, python);
    lua_settag(L, pyntag);

    set_table_string(L, python, PY_UNICODE_ENCODING, "utf8");
    set_table_string(L, python, PY_UNICODE_ENCODING_ERRORHANDLER, "strict");
    set_table_number(L, python, PY_OBJECT_BY_REFERENCE, 0);
    set_table_number(L, python, PY_API_IS_EMBEDDED, 0);  // If Python is inside Lua
    set_table_number(L, python, PY_LUA_TABLE_CONVERT, 0); // table convert ?

    lua_pushcfunction(L, py_args);
    lua_setglobal(L, PY_ARGS_FUNC);

    lua_pushcfunction(L, py_kwargs);
    lua_setglobal(L, PY_KWARGS_FUNC);

    lua_pushcfunction(L, py_args_array);
    lua_setglobal(L, PY_ARGS_ARRAY_FUNC);

    lua_pushobject(L, python);
    lua_setglobal(L, PY_API_NAME);  // api python

    int index = 0;
    while (py_lib[index].name) {
        set_table_fn(L, python, py_lib[index].name, py_lib[index].func);
        index++;
    }

    // register all tag methods
    int ntag = lua_newtag(L);
    index = 0;
    while (lua_tag_methods[index].name) {
        lua_pushcfunction(L, lua_tag_methods[index].func);
        lua_settagmethod(L, ntag, lua_tag_methods[index].name);
        index++;
    }

    // tag event
    set_table_number(L, python, PY_API_TAG, ntag);

    PyObject *pyObject = Py_True;
    Py_INCREF(pyObject);
    set_table_usertag(L, python, PY_TRUE, py_object_container(L, pyObject, 0), ntag);

    pyObject = Py_False;
    Py_INCREF(pyObject);
    set_table_usertag(L, python, PY_FALSE, py_object_container(L, pyObject, 0), ntag);

    pyObject = Py_None;
    Py_INCREF(pyObject);
    set_table_usertag(L, python, PY_NONE, py_object_container(L, pyObject, 0), ntag);
    return 0;
}

/* Initialize Python interpreter */
static void python_system_init(lua_State *L) {
    char *python_home = luaL_check_string(L, 1);
    if (!Py_IsInitialized()) {
        python_setnumber(L, PY_API_IS_EMBEDDED, 1); // If python is inside Lua
        if (PyType_Ready(&LuaObject_Type) == 0) {
            Py_INCREF(&LuaObject_Type);
        } else {
            lua_error(L, "failure initializing lua object type");
        }
        PyObject *luam, *mainm, *maind;
#if PY_MAJOR_VERSION >= 3
        wchar_t *argv[] = {L"<lua_bootstrap>", 0};
#else
        char *argv[] = {"<lua_bootstrap>", 0};
#endif
        Py_SetProgramName(argv[0]);
        Py_SetPythonHome(python_home);
        Py_InitializeEx(0);
        PySys_SetArgv(1, argv);
        /* Import 'lua' automatically. */
        luam = PyImport_ImportModule("lua_bootstrap");
        if (!luam) {
            lua_error(L, "Can't import lua_bootstrap module");
        } else {
            mainm = PyImport_AddModule("__main__");
            if (!mainm) {
                lua_error(L, "Can't get __main__ module");
            } else {
                maind = PyModule_GetDict(mainm);
                PyDict_SetItemString(maind, "lua_bootstrap", luam);
                Py_DECREF(luam);
            }
        }
    }
}
