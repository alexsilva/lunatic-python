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
#include <lualib.h>

#include "pythoninlua.h"
#include "luainpython.h"

#include "pyconv.h"
#include "luaconv.h"

#ifndef lua_next
#include "lapi.h"
#else
#include "lshared.h"
#endif

lua_State *LuaState = NULL;

static PyObject *LuaCall(lua_State *L, lua_Object lobj, PyObject *args) {
    PyObject *ret = NULL;
    PyObject *arg;
    int nargs, rc, i;

    if (!PyTuple_Check(args)) {
        PyErr_SetString(PyExc_TypeError, "tuple expected");
        return NULL;
    }

    nargs = PyTuple_Size(args);
    for (i = 0; i != nargs; i++) {
        arg = PyTuple_GetItem(args, i);
        if (arg == NULL) {
            PyErr_Format(PyExc_TypeError,
                     "failed to get tuple item #%d", i);
            return NULL;
        }
        rc = py_convert(L, arg);
        if (!rc) {
            PyErr_Format(PyExc_TypeError,
                     "failed to convert argument #%d", i);
            return NULL;
        }
    }
    if (lua_callfunction(L, lobj)) {
        char *name;  // get function name
        lua_getobjname(L, lobj, &name);
        PyErr_Format(PyExc_Exception, "calling function \"%s\"", name);
        return NULL;
    }

    nargs = lua_gettop(L);
    if (nargs == 1) {
        ret = lua_convert(L, 1);
        if (!ret) {
            PyErr_SetString(PyExc_TypeError,
                        "failed to convert return");
            Py_DECREF(ret);
            return NULL;
        }
    } else if (nargs > 1) {
        ret = PyTuple_New(nargs);
        if (!ret) {
            PyErr_SetString(PyExc_RuntimeError,
                    "failed to create return tuple");
            return NULL;
        }
        for (i = 0; i != nargs; i++) {
            arg = lua_convert(L, i+1);
            if (!arg) {
                PyErr_Format(PyExc_TypeError,
                         "failed to convert return #%d", i);
                Py_DECREF(ret);
                return NULL;
            }
            PyTuple_SetItem(ret, i, arg);
        }
    } else {
        Py_INCREF(Py_None);
        ret = Py_None;
    }
    return ret;
}

static void LuaObject_dealloc(LuaObject *self)
{
    lua_unref(LuaState, self->ref);
    if (self->refiter)
        lua_unref(LuaState, self->refiter);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *LuaObject_getattr(PyObject *obj, PyObject *attr)
{
    lua_Object ltable = lua_getref(LuaState, ((LuaObject*)obj)->ref);
    if (lua_isnil(LuaState, ltable)) {
        lua_pop(LuaState);
        PyErr_SetString(PyExc_RuntimeError, "lost reference");
        return NULL;
    }
    
    if (!lua_isstring(LuaState, ltable)
        && !lua_istable(LuaState, ltable)
        && !lua_isuserdata(LuaState, ltable))
    {
        lua_pop(LuaState);

        PyErr_SetString(PyExc_RuntimeError, "not an indexable value");
        return NULL;
    }

    PyObject *ret = NULL;
    lua_pushobject(LuaState, ltable); // push table
    int rc = py_convert(LuaState, attr); // push key
    if (rc) {
        lua_Object lobj = lua_gettable(LuaState);
        ret = lua_obj_convert(LuaState, 0, lobj); // convert
    } else {
        PyErr_SetString(PyExc_ValueError, "can't convert attr/key");
    }
    return ret;
}

static int LuaObject_setattr(PyObject *obj, PyObject *attr, PyObject *value) {
    lua_beginblock(LuaState);
    int ret = -1;
    int rc;
    lua_Object ltable = lua_getref(LuaState, ((LuaObject*)obj)->ref);
    if (lua_isnil(LuaState, ltable)) {
        lua_pop(LuaState);
        PyErr_SetString(PyExc_RuntimeError, "lost reference");
        return -1;
    }
    if (!lua_istable(LuaState, ltable)) {
        lua_pop(LuaState);
        PyErr_SetString(PyExc_TypeError, "Lua object is not a table");
        return -1;
    }
    lua_pushobject(LuaState, ltable); // push table
    rc = py_convert(LuaState, attr);
    if (rc) {
        if (NULL == value) {
            lua_pushnil(LuaState);
            rc = 1;
        } else {
            rc = py_convert(LuaState, value); // push value ?
        }
        if (rc) {
            lua_settable(LuaState);
            ret = 0;
        } else {
            PyErr_SetString(PyExc_ValueError,
                    "can't convert value");
        }
    } else {
        PyErr_SetString(PyExc_ValueError, "can't convert key/attr");
    }
    lua_endblock(LuaState);
    return ret;
}

static PyObject *LuaObject_str(PyObject *obj) {
    lua_Object lobj = lua_getref(LuaState, ((LuaObject*)obj)->ref);
    TObject *o = lapi_address(LuaState, lobj);
    char buff[64];
    switch (ttype(o)) { // Lua 3.2 source code builtin.c
        case LUA_T_NUMBER:
            return PyString_FromFormat("<Lua number %ld>", (long) lua_getnumber(LuaState, lobj));
        case LUA_T_STRING:
            return PyString_FromFormat("<Lua string size %ld>", lua_strlen(LuaState, lobj));
        case LUA_T_ARRAY:
            sprintf(buff, "<Lua table at %p>", (void *)o->value.a);
            break;
        case LUA_T_CLOSURE:
            sprintf(buff, "<Lua function at %p>", (void *)o->value.cl);
            break;
        case LUA_T_PROTO:
            sprintf(buff, "<Lua function at %p>", (void *)o->value.tf);
            break;
        case LUA_T_CPROTO:
            sprintf(buff, "<Lua function at %p>", (void *)o->value.f);
            break;
        case LUA_T_USERDATA:
            sprintf(buff, "<Lua userdata at %p>", o->value.ts->u.d.v);
            break;
        case LUA_T_NIL:
            return PyString_FromString("nil");
        default:
            return PyString_FromString("invalid type");
    }
    return PyString_FromString(buff);
}

static PyObject *LuaObject_call(PyObject *obj, PyObject *args)
{
    lua_Object lobj = lua_getref(LuaState, ((LuaObject*)obj)->ref);

    return LuaCall(LuaState, lobj, args);
}

static PyObject *LuaObject_iternext(LuaObject *obj)
{
//    PyObject *ret = NULL;
//    lua_rawgeti(LuaState, LUA_REGISTRYINDEX, ((LuaObject*)obj)->ref);
//
//    if (obj->refiter == 0)
//        lua_pushnil(LuaState);
//    else
//        lua_rawgeti(LuaState, LUA_REGISTRYINDEX, obj->refiter);
//
//    if (lua_next(LuaState, -2) != 0) {
//        /* Remove value. */
//        lua_pop(LuaState, 1);
//        ret = LuaConvert(LuaState, -1);
//        /* Save key for next iteration. */
//        if (!obj->refiter)
//            obj->refiter = luaL_ref(LuaState, LUA_REGISTRYINDEX);
//        else
//            lua_rawseti(LuaState, LUA_REGISTRYINDEX, obj->refiter);
//    } else if (obj->refiter) {
//        luaL_unref(LuaState, LUA_REGISTRYINDEX, obj->refiter);
//        obj->refiter = 0;
//    }

    return NULL;
}

static int LuaObject_length(LuaObject *obj) {
    int len = 0;
    lua_Object lobj = lua_getref(LuaState, obj->ref);
    if (lua_isfunction(LuaState, lobj)) {
        len = 1;  // 1 is True
    } else if (lua_isstring(LuaState, lobj)) {
        len = lua_strlen(LuaState, lobj);
    } else if (lua_istable(LuaState, lobj)) {
        lua_pushobject(LuaState, lobj);
        lua_call(LuaState, "getn");
        len = (int) lua_getnumber(LuaState, lua_getresult(LuaState, 1));
    }
    return len;
}

static PyObject *LuaObject_subscript(PyObject *obj, PyObject *key)
{
    return LuaObject_getattr(obj, key);
}

static int LuaObject_ass_subscript(PyObject *obj, PyObject *key, PyObject *value)
{
    return LuaObject_setattr(obj, key, value);
}

static PyMappingMethods LuaObject_as_mapping = {
#if PY_VERSION_HEX >= 0x02050000
    (lenfunc)LuaObject_length,    /*mp_length*/
#else
    (inquiry)LuaObject_length,    /*mp_length*/
#endif
    (binaryfunc)LuaObject_subscript,/*mp_subscript*/
    (objobjargproc)LuaObject_ass_subscript,/*mp_ass_subscript*/
};

PyTypeObject LuaObject_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "lua.custom",             /*tp_name*/
    sizeof(LuaObject),        /*tp_basicsize*/
    0,                        /*tp_itemsize*/
    (destructor)LuaObject_dealloc, /*tp_dealloc*/
    0,                        /*tp_print*/
    0,                        /*tp_getattr*/
    0,                        /*tp_setattr*/
    0,                        /*tp_compare*/
    LuaObject_str,            /*tp_repr*/
    0,                        /*tp_as_number*/
    0,                        /*tp_as_sequence*/
    &LuaObject_as_mapping,    /*tp_as_mapping*/
    0,                        /*tp_hash*/
    (ternaryfunc)LuaObject_call,     /*tp_call*/
    LuaObject_str,            /*tp_str*/
    LuaObject_getattr,       /*tp_getattro*/
    LuaObject_setattr,        /*tp_setattro*/
    0,                        /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "custom lua object",      /*tp_doc*/
    0,                        /*tp_traverse*/
    0,                        /*tp_clear*/
    0,                        /*tp_richcompare*/
    0,                        /*tp_weaklistoffset*/
    PyObject_SelfIter,        /*tp_iter*/
    (iternextfunc)LuaObject_iternext, /*tp_iternext*/
    0,                        /*tp_methods*/
    0,                        /*tp_members*/
    0,                        /*tp_getset*/
    0,                        /*tp_base*/
    0,                        /*tp_dict*/
    0,                        /*tp_descr_get*/
    0,                        /*tp_descr_set*/
    0,                        /*tp_dictoffset*/
    0,                        /*tp_init*/
    PyType_GenericAlloc,      /*tp_alloc*/
    PyType_GenericNew,        /*tp_new*/
    PyObject_Del,            /*tp_free*/
    0,                        /*tp_is_gc*/
};


PyObject *Lua_run(PyObject *args, int eval) {
    lua_beginblock(LuaState);
    PyObject *ret = NULL;
    char *buf = NULL;
    char *s;
    int len;

    if (!PyArg_ParseTuple(args, "s#", &s, &len))
        return NULL;

    if (eval) {
        char *prefix = "return ";
        buf = (char *) malloc(strlen(prefix) + len + 2);
        strcpy(buf, prefix);
        strncat(buf, s, (size_t) len);
        strncat(buf, ";", 1);
        s = buf;
        len = strlen(prefix) + len;
    }
    if (lua_dobuffer(LuaState, s, len, "<python>") != 0) {
        PyErr_Format(PyExc_RuntimeError,
                 "error loading code: %s", s);
        return NULL;
    }
    if (eval) free(buf);
    int nargs = lua_gettop(LuaState);
    if (nargs > 0) ret = lua_convert(LuaState, 1);
    if (!ret) {
        Py_INCREF(Py_None);
        ret = Py_None;
    } else {
        Py_INCREF(ret);
    }
    lua_endblock(LuaState);
    return ret;
}

PyObject *Lua_execute(PyObject *self, PyObject *args)
{
    return Lua_run(args, 0);
}

PyObject *Lua_eval(PyObject *self, PyObject *args)
{
    return Lua_run(args, 1);
}

PyObject *Lua_globals(PyObject *self, PyObject *args)
{
    PyObject *ret = NULL;
    lua_Object lobj = lua_getglobal(LuaState, "_G");
    if (lua_isnil(LuaState, lobj)) {
        PyErr_SetString(PyExc_RuntimeError,
                "lost globals reference");
        return NULL;
    }
    ret = lua_convert(LuaState, 1);
    if (!ret)
        PyErr_Format(PyExc_TypeError,
                 "failed to convert globals table");
    return ret;
}

static PyObject *Lua_require(PyObject *self, PyObject *args)
{
    lua_Object lobj = lua_getglobal(LuaState, "dofile");
    if (lua_isnil(LuaState, lobj)) {
        PyErr_SetString(PyExc_RuntimeError, "require is not defined");
        return NULL;
    }
    return LuaCall(LuaState, lobj, args);
}

static PyMethodDef lua_methods[] = {
    {"execute",    Lua_execute,    METH_VARARGS,        NULL},
    {"eval",       Lua_eval,       METH_VARARGS,        NULL},
    {"globals",    Lua_globals,    METH_NOARGS,         NULL},
    {"require",    Lua_require,    METH_VARARGS,        NULL},
    {NULL,         NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef lua_module = {
    PyModuleDef_HEAD_INIT,
    "lua",
    "Lunatic-Python Python-Lua bridge",
    -1,
    lua_methods
};
#endif

PyMODINIT_FUNC PyInit_lua(void)
{
    PyObject *m;

#if PY_MAJOR_VERSION >= 3
    if (PyType_Ready(&LuaObject_Type) < 0) return NULL;
    m = PyModule_Create(&lua_module);
    if (m == NULL) return NULL;
#else
    if (PyType_Ready(&LuaObject_Type) < 0) return;
    m = Py_InitModule3("lualib", lua_methods,
                       "Lunatic-Python Python-Lua bridge");
    if (m == NULL) return;
#endif

    Py_INCREF(&LuaObject_Type);

    if (!LuaState) {
        LuaState = lua_open();

        // default libs
        lua_iolibopen(LuaState);
        lua_strlibopen(LuaState);
        lua_mathlibopen(LuaState);

        luaopen_python(LuaState);
    }

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
