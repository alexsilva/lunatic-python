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
#include <lapi.h>

#include "pythoninlua.h"

static int py_asfunc_call(lua_State *L);

// ----------------------------------------

int luaL_error(lua_State *L, char *msg) {
    lua_pushstring(L, msg);
    return 0;
}

int lua_gettop(lua_State *L) {
    return L->Cstack.num;
}

int lua_isboolean(lua_State *L, lua_Object obj) {
    return 0; //Todo:
}

int lua_getboolean(lua_State *L, lua_Object obj) {
    return 0; //Todo:
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

static PyObject *LuaConvert(lua_State *L, int n) {
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
        ret = PyUnicode_FromStringAndSize(s, len);

    } else if (lua_istable(L, lobj)) {
        py_object *pobj = get_py_object(L, n);
        ret = pobj->o;
        free(pobj);

    } else if (lua_isboolean(L, lobj)) {
        if (lua_getboolean(L, lobj)) {
            Py_INCREF(Py_True);
            ret = Py_True;
        } else {
            Py_INCREF(Py_False);
            ret = Py_False;
        }
    } else if (lua_isuserdata(L, lobj)) {
        py_object *obj = luaPy_to_pobject(L, n);
        if (obj) {
            Py_INCREF(obj->o);
            ret = obj->o;
        }
        /* Otherwise go on and handle as custom. */
    }
    return ret;
}

// ----------------------------------------
static void lpy_object_index(lua_State *L);
static void lpy_object_call(lua_State *L);
static void py_object_newindex_set(lua_State *L);
static void py_object_gc(lua_State *L);

static struct luaL_reg lua_tag_methods[] = {
    {"function",  lpy_object_call},
    {"index", lpy_object_index},
    {"settable", py_object_newindex_set},
    {"gc", py_object_gc},
    {NULL, NULL}
};

static int py_convert_custom(lua_State *L, PyObject *pobj, int asindx) {
    Py_INCREF(pobj);

    lua_Object ltable = lua_createtable(L);

    // insert table
    lua_pushobject(L, ltable);
    lua_pushstring(L, POBJECT);
    lua_pushuserdata(L, pobj);
    lua_settable(L);

    lua_pushobject(L, ltable);
    lua_pushstring(L, ASINDX);
    lua_pushnumber(L, asindx);
    lua_settable(L);

    // register all tag methods
    int ntag = lua_newtag(L);
    int index = 0;
    while (lua_tag_methods[index].name) {
        lua_pushcfunction(L, lua_tag_methods[index].func);
        lua_settagmethod(L, ntag, lua_tag_methods[index].name);
        index++;
    }
    // set tag
    lua_pushobject(L, ltable);
    lua_settag(L, ntag);

    // returning table
    lua_pushobject(L, ltable);
    return 1;
}

int py_convert(lua_State *L, PyObject *o, int withnone) {
    int ret = 0;
    if (o == Py_None) {
        if (withnone) {
            lua_pushstring(L, "Py_None");
            //Todo: lua_rawget(L, LUA_REGISTRYINDEX);
            if (lua_isnil(L, -1)) {
                //  lua_pop(L, 1);
                luaL_error(L, "lost none from registry");
            }
        } else {
            /* Not really needed, but this way we may check
             * for errors with ret == 0. */
            lua_pushnil(L);
            ret = 1;
        }
    } else if (o == Py_True) {
        lua_pushnumber(L, 1);
        ret = 1;
    } else if (o == Py_False) {
        lua_pushnil(L);
        ret = 1;
#if PY_MAJOR_VERSION >= 3
        } else if (PyUnicode_Check(o)) {
        Py_ssize_t len;
        char *s = PyUnicode_AsUTF8AndSize(o, &len);
#elif defined(PyUnicode_Check)
    } else if (PyString_Check(o)  || PyUnicode_Check(o)) {
#else
    } else if (PyString_Check(o)) {
#endif
        Py_ssize_t len;
        char *s;
        PyString_AsStringAndSize(o, &s, &len);

        lua_pushlstring(L, s, len);
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
    } else if (LuaObject_Check(o)) {
        //lua_rawgeti(L, LUA_REGISTRYINDEX, ((LuaObject*)o)->ref);
        lua_error(L, "lua object: no implemented!");
        ret = 1;
    } else {
        int asindx = 0;
        if (PyList_Check(o) || PyTuple_Check(o))
            asindx = 1;
        ret = py_convert_custom(L, o, asindx);

        //if (ret && !asindx && (PyFunction_Check(o) || PyCFunction_Check(o))) {
        //    lua_pushcclosure(L, py_asfunc_call, 1);
        //}
    }
    return ret;
}

static int py_object_call(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    PyObject *args;
    PyObject *value;

    int nargs = lua_gettop(L)-1;
    int ret = 0;
    int i;

    if (!pobj) {
        luaL_argerror(L, 1, "not a python object");
    }
    if (!PyCallable_Check(pobj->o)) {
        lua_error(L, "object is not callable");
        return 0;
    }

    args = PyTuple_New(nargs);
    if (!args) {
        PyErr_Print();
        lua_error(L, "failed to create arguments tuple");
        return 0;
    }

    for (i = 0; i != nargs; i++) {
        PyObject *arg = LuaConvert(L, i+2);
        if (!arg) {
            Py_DECREF(args);
            char *error = "failed to convert argument #%d";
            char buff[strlen(error) + 10];
            sprintf(buff, error, i+1);
            lua_error(L, &buff[0]);
        }
        PyTuple_SetItem(args, i, arg);
    }

    value = PyObject_CallObject(pobj->o, args);
    if (value) {
        ret = py_convert(L, value, 0);
        Py_DECREF(value);
    } else {
        PyErr_Print();
        lua_error(L, "error calling python function");
    }
    free(pobj);
    return ret;
}

static void lpy_object_call(lua_State *L) {
    py_object_call(L);
}

static int _p_object_newindex_set(lua_State *L, py_object *obj, int keyn, int valuen) {
    PyObject *value;
    PyObject *key = LuaConvert(L, keyn);
    if (!key) luaL_argerror(L, 1, "failed to convert key");

    lua_Object lobj = lua_getparam(L, valuen);

    if (!lua_isnil(L, lobj)) {
        value = LuaConvert(L, valuen);
        if (!value) {
            Py_DECREF(key);
            luaL_argerror(L, 1, "failed to convert value");
        }
        // setitem (obj[0] = 1) if int else setattr(obj.val = 1)
        if (PyInt_Check(key)) {
            if (PyObject_SetItem(obj->o, key, value) == -1) {
                PyErr_Print();
                luaL_error(L, "failed to set item");
            }
        } else if (PyObject_SetAttr(obj->o, key, value) == -1) {
            PyErr_Print();
            luaL_error(L, "failed to set item");
        }
        Py_DECREF(value);
    } else {
        if (PyObject_DelItem(obj->o, key) == -1) {
            PyErr_Print();
            luaL_error(L, "failed to delete item");
        }
    }

    Py_DECREF(key);
    return 0;
}

static void py_object_newindex_set(lua_State *L) {
    py_object *obj = get_py_object(L, 1);
    if (lua_gettop(L) < 2) {
        luaL_error(L, "invalid arguments");
        return;
    }
    _p_object_newindex_set(L, obj, 2, 3);
}

static void py_object_newindex(lua_State *L) {
    py_object *obj = get_py_object(L, 1);
    const char *attr;
    PyObject *value;

    if (!obj) {
        luaL_argerror(L, 1, "not a python object");
    }

    if (obj->asindx) {
        _p_object_newindex_set(L, obj, 2, 3);
        return;
    }
    attr = luaL_check_string(L, 2);
    if (!attr) {
        luaL_argerror(L, 2, "string needed");
    }

    value = LuaConvert(L, 3);
    if (!value) {
        luaL_argerror(L, 1, "failed to convert value");
    }

    if (PyObject_SetAttrString(obj->o, (char*)attr, value) == -1) {
        Py_DECREF(value);
        PyErr_Print();
        luaL_error(L, "failed to set value");
    }

    Py_DECREF(value);
}

static int _p_object_index_get(lua_State *L, py_object *pobj, int keyn) {
    PyObject *key = LuaConvert(L, keyn);
    PyObject *item;
    int ret = 0;

    if (!key) luaL_argerror(L, 1, "failed to convert key");

    if (PyObject_HasAttr(pobj->o, key)) {
        item = PyObject_GetAttr(pobj->o, key);
    } else {
        item = PyObject_GetItem(pobj->o, key);
    }
    Py_DECREF(key);

    if (item) {
        ret = py_convert(L, item, 0);
        Py_DECREF(item);
    } else {
        PyErr_Clear();
        if (lua_gettop(L) > keyn) {
            //lua_pushvalue(L, keyn+1);
            ret = 1;
        }
    }
    return ret;
}

static int py_object_index(lua_State *L) {
    py_object *obj = get_py_object(L, 1);
    if (!obj) luaL_argerror(L, 1, "not a python object");
    return _p_object_index_get(L, obj, 2);
}

static void lpy_object_index(lua_State *L) {
    py_object_index(L);
}

static void py_object_gc(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    if (pobj) {
        Py_DECREF(pobj->o);
    }
    free(pobj);
}

static void py_object_tostring(lua_State *L) {
    py_object *obj = get_py_object(L, 1);

    if (obj) {
        PyObject *repr = PyObject_Str(obj->o);
        if (!repr) {
            char buf[256];
            snprintf(buf, 256, "python object: %p", obj->o);
            lua_pushstring(L, buf);
            PyErr_Clear();
        } else {
            py_convert(L, repr, 0);
            //Todo: assert(lua_type(L, -1) == LUA_TSTRING);
            Py_DECREF(repr);
        }
    }
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
        len = strlen(s)+1;
        buffer = (char *) malloc(len+1);
        if (!buffer) {
            return luaL_error(L, "Failed allocating buffer for execution");
        }
        strcpy(buffer, s);
        buffer[len-1] = '\n';
        buffer[len] = '\0';
        s = buffer;
    }

    m = PyImport_AddModule("__main__");
    if (!m) {
        free(buffer);
        return luaL_error(L, "Can't get __main__ module");
    }
    d = PyModule_GetDict(m);

    o = PyRun_StringFlags(s, eval ? Py_eval_input : Py_single_input,
                          d, d, NULL);

    free(buffer);

    if (!o) {
        PyErr_Print();
        return 0;
    }

    if (py_convert(L, o, 0))
        ret = 1;

    Py_DECREF(o);

#if PY_MAJOR_VERSION < 3
    if (Py_FlushLine())
#endif
        PyErr_Clear();

    return ret;
}

static int py_execute(lua_State *L) {
    return py_run(L, 0);
}

static void lpy_execute(lua_State *L) {
    py_execute(L);  // return ignored
}

static int py_eval(lua_State *L) {
    return py_run(L, 1);
}

static void lpy_eval(lua_State *L) {
    py_eval(L); // return ignored
}

static int py_asindx(lua_State *L) {
    py_object *obj = get_py_object(L, 1);
    if (!obj) {
        luaL_argerror(L, 1, "not a python object");
    }
    return py_convert_custom(L, obj->o, 1);
}

static int py_asattr(lua_State *L) {
    py_object *pobj = get_py_object(L, 1);
    if (!pobj) {
        luaL_argerror(L, 1, "not a python object");
    }
    return py_convert_custom(L, pobj->o, 0);
}

static int py_asfunc_call(lua_State *L) {
    // Todo:
    // lua_pushvalue(L, lua_upvalueindex(1));
    // lua_insert(L, 1);

    return py_object_call(L);
}

static int py_asfunc(lua_State *L) {
    int ret = 0;
    if (get_py_object(L, 1)) {
        // lua_pushcclosure(L, py_asfunc_call, 1);
        ret = 1;
    } else {
        luaL_argerror(L, 1, "not a python object");
    }
    return ret;
}

static int py_globals(lua_State *L) {
    PyObject *globals;
    if (lua_gettop(L) != 0) {
        return luaL_error(L, "invalid arguments");
    }
    globals = PyEval_GetGlobals();
    if (!globals) {
        PyObject *module = PyImport_AddModule("__main__");
        if (!module) {
            return luaL_error(L, "Can't get __main__ module");
        }
        globals = PyModule_GetDict(module);
    }

    if (!globals) {
        PyErr_Print();
        return luaL_error(L, "can't get globals");
    }

    return py_convert_custom(L, globals, 1);
}

static void lpy_globals(lua_State *L) {
    py_globals(L);
}

static int py_locals(lua_State *L) {
    PyObject *locals;

    if (lua_gettop(L) != 0) {
        return luaL_error(L, "invalid arguments");
    }

    locals = PyEval_GetLocals();
    if (!locals) return py_globals(L);

    return py_convert_custom(L, locals, 1);
}

static void lpy_locals(lua_State *L) {
    py_locals(L);
}

static int py_builtins(lua_State *L) {
    PyObject *builtins;

    if (lua_gettop(L) != 0) {
        return luaL_error(L, "invalid arguments");
    }

    builtins = PyEval_GetBuiltins();
    if (!builtins) {
        PyErr_Print();
        return luaL_error(L, "failed to get builtins");
    }

    return py_convert_custom(L, builtins, 1);
}

static void lpy_builtins(lua_State *L) {
    py_builtins(L);
}

static int py_import(lua_State *L) {
    const char *name = luaL_check_string(L, 1);
    PyObject *module;
    int ret;

    if (!name) luaL_argerror(L, 1, "module name expected");

    module = PyImport_ImportModule((char*) name);

    if (!module) {
        PyErr_Print();
        char *error = "failed importing '%s'";
        char buff[strlen(error) + strlen(name) + 1];
        sprintf(buff, error, name);
        lua_error(L, &buff[0]);
    }

    ret = py_convert_custom(L, module, 0);
    Py_DECREF(module);
    return ret;
}

static void lpy_import(lua_State *L) {
    py_import(L);
}

py_object* luaPy_to_pobject(lua_State *L, int n) {
    lua_Object lobj = lua_getparam(L, n);

    //Todo:
    //if(!lua_getmetatable(L, n)) {
    //    return NULL;
    //}

    //luaL_getmetatable(L, POBJECT);

    //int is_pobject = lua_rawequal(L, -1, -2);

    //lua_pop(L, 2);

    //return is_pobject ? (py_object *) lua_touserdata(L, n) : NULL;

    return NULL;
}

void python_system_init(lua_State *L);

static struct luaL_reg py_lib[] = {
        {"execute", lpy_execute},
        {"eval", lpy_eval},
//    {"asindx",  py_asindx},
//    {"asattr",  py_asattr},
//    {"asfunc",  py_asfunc},
        {"locals",  lpy_locals},
        {"globals", lpy_globals},
        {"builtins", lpy_builtins},
        {"import",  lpy_import},
        {"system_init", python_system_init},
        {NULL, NULL}
};


#define set_table(L, obj, name, value) \
    lua_pushobject(L, obj); \
    lua_pushstring(L, name); \
    lua_pushcfunction(L, value); \
    lua_settable(L);

/* Register module */
LUA_API int luaopen_python(lua_State *L) {
    lua_Object python = lua_createtable(L);

    lua_pushobject(L, python);
    lua_setglobal(L, "python");  // api python

    int index = 0;
    while (py_lib[index].name) {
        set_table(L, python, py_lib[index].name, py_lib[index].func);
        index++;
    }
    // try startup system
    // python_system_init(L);
    return 0;
}

/* Initialize Python interpreter */
void python_system_init(lua_State *L) {
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
            luaL_error(L, "Can't import lua module");
        } else {
            mainm = PyImport_AddModule("__main__");
            if (!mainm) {
                luaL_error(L, "Can't get __main__ module");
            } else {
                maind = PyModule_GetDict(mainm);
                PyDict_SetItemString(maind, "lua", luam);
                Py_DECREF(luam);
            }
        }
    }
    /* Register 'none' */
    lua_pushstring(L, "Py_None");
    int rc = py_convert_custom(L, Py_None, 0);
    if (rc) {
        lua_pushstring(L, "none");
        //Todo:
        //lua_pushvalue(L, -2);
        //lua_rawset(L, -5); /* python.none */
        //lua_rawset(L, LUA_REGISTRYINDEX); /* registry.Py_None */
    } else {
        //Todo: lua_pop(L, 1);
        luaL_error(L, "failed to convert none object");
    }
}
