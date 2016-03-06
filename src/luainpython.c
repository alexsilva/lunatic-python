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

#include "pythoninlua.h"
#include "luainpython.h"
#include "pyconv.h"
#include "utils.h"

#ifndef lua_next
#include "lapi.h"
#else
#include "lshared.h"
#endif

#ifdef CGILUA_ENV
#include "cgilua.h"
#else
#include <lualib.h>
#endif

#define LUA_EXT_VERSION "1.0.0"


static PyObject *LuaCall(LuaObject *self, lua_Object lobj, PyObject *args) {
    if (!PyTuple_Check(args)) {
        PyErr_SetString(PyExc_TypeError, "tuple expected");
        return NULL;
    }
    PyObject *arg;
    int nargs, index;
    nargs = PyTuple_Size(args);
    for (index = 0; index < nargs; index++) {
        arg = PyTuple_GetItem(args, index);
        if (arg == NULL) {
            PyErr_Format(PyExc_TypeError, "failed to get tuple item #%d", index);
            return NULL;
        }
        if (!isvalidstatus(py_convert(self->interpreter->L, arg))) {
            PyErr_Format(PyExc_TypeError, "failed to convert argument #%d", index);
            return NULL;
        }
    }
    if (lua_callfunction(self->interpreter->L, lobj)) {
        char *name;  // get function name
        lua_getobjname(self->interpreter->L, lobj, &name);
        name = name ? name : "...";
        char *format = "call function lua (%s)";
        char buff[buffsize_calc(2, format, name)];
        sprintf(buff, format, name);
        python_new_error(PyExc_RuntimeError, &buff[0]);
        return NULL;
    }
    PyObject *ret;
    nargs = lua_gettop(self->interpreter->L);
    if (nargs == 1) {
        ret = lua_interpreter_stack_convert(self->interpreter, 1);
        if (!ret) {
            PyErr_SetString(PyExc_TypeError, "failed to convert return");
            return NULL;
        }
    } else if (nargs > 1) {
        ret = PyTuple_New(nargs);
        if (!ret) {
            PyErr_SetString(PyExc_RuntimeError, "failed to create return tuple");
            return NULL;
        }
        for (index = 0; index != nargs; index++) {
            arg = lua_interpreter_stack_convert(self->interpreter, index + 1);
            if (!arg) {
                PyErr_Format(PyExc_TypeError, "failed to convert return #%d", index);
                Py_DECREF(ret);
                return NULL;
            }
            PyTuple_SetItem(ret, index, arg);
        }
    } else {
        Py_INCREF(Py_None);
        ret = Py_None;
    }
    return ret;
}

static void LuaObject_dealloc(LuaObject *self) {
    lua_beginblock(self->interpreter->L);
    lua_unref(self->interpreter->L, self->ref);
    lua_endblock(self->interpreter->L);
    if (!self->interpreter->isPyType) {
        self->interpreter->L = NULL;
        free(self->interpreter);
    } else {
        Py_DECREF(self->interpreter);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *LuaObject_getattr(LuaObject *self, PyObject *attr) {
    lua_beginblock(self->interpreter->L);
    lua_Object ltable = lua_getref(self->interpreter->L, self->ref);
    if (lua_isnil(self->interpreter->L, ltable)) {
        PyErr_SetString(PyExc_RuntimeError, "lost reference");
        return NULL;
    }
    if (!lua_istable(self->interpreter->L, ltable) && !lua_isuserdata(self->interpreter->L, ltable)) {
        PyErr_SetString(PyExc_RuntimeError, "not an indexable value");
        return NULL;
    }
    PyObject *ret = NULL;
    lua_pushobject(self->interpreter->L, ltable); // push table
    if (py_convert(self->interpreter->L, attr) != UNCHANGED) { // push key
        lua_Object lobj = lua_gettable(self->interpreter->L);
        ret = lua_interpreter_object_convert(self->interpreter, 0, lobj); // convert
        if (is_object_container(self->interpreter->L, lobj)) {
            // The object will be shared with the python which in turn will remove a reference.
            // python does not know that the lua is managing the reference.
            Py_INCREF(ret);
        }
    } else {
        PyErr_SetString(PyExc_ValueError, "can't convert attr/key");
    }
    lua_endblock(self->interpreter->L);
    return ret;
}

static int LuaObject_setattr(LuaObject *self, PyObject *attr, PyObject *value) {
    lua_beginblock(self->interpreter->L);
    int ret = -1;
    lua_Object ltable = lua_getref(self->interpreter->L, self->ref);
    if (lua_isnil(self->interpreter->L, ltable)) {
        PyErr_SetString(PyExc_RuntimeError, "lost reference");
        return ret;
    }
    if (!lua_istable(self->interpreter->L, ltable)) {
        PyErr_SetString(PyExc_TypeError, "Lua object is not a table");
        return ret;
    }
    lua_pushobject(self->interpreter->L, ltable); // push table
    Conversion res = py_convert(self->interpreter->L, attr);
    if (isvalidstatus(res)) {
        if (value == NULL) {
            lua_pushnil(self->interpreter->L);
            res = CONVERTED;
        } else {
            res = py_convert(self->interpreter->L, value); // push value ?
        }
        if (isvalidstatus(res)) {
            lua_settable(self->interpreter->L);
            ret = 0;
        } else {
            PyErr_SetString(PyExc_ValueError, "can't convert value");
        }
    } else {
        PyErr_SetString(PyExc_ValueError, "can't convert key/attr");
    }
    lua_endblock(self->interpreter->L);
    return ret;
}

static PyObject *LuaObject_str(LuaObject *self) {
    lua_beginblock(self->interpreter->L);
    lua_Object lobj = lua_getref(self->interpreter->L, self->ref);
    TObject *o = lapi_address(self->interpreter->L, lobj);
    char buff[64];
    switch (ttype(o)) { // Lua 3.2 source code builtin.c
        case LUA_T_NUMBER:
            return PyString_FromFormat("<Lua number %ld>", (long) lua_getnumber(self->interpreter->L, lobj));
        case LUA_T_STRING:
            return PyString_FromFormat("<Lua string size %ld>", lua_strlen(self->interpreter->L, lobj));
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
    lua_endblock(self->interpreter->L);
    return PyString_FromString(buff);
}

static PyObject *LuaObject_call(LuaObject *self, PyObject *args) {
    lua_beginblock(self->interpreter->L);
    lua_Object lobj = lua_getref(self->interpreter->L, self->ref);
    PyObject *ret = LuaCall(self, lobj, args);
    lua_endblock(self->interpreter->L);
    return ret;
}

/* LuaObject iterator types */
typedef struct {
    PyObject_HEAD
    LuaObject *luaobject; /* Set to NULL when iterator is exhausted */
    Py_ssize_t refiter;
} luaiterobject;

static PyObject *LuaObjectIter_next(luaiterobject *li) {
    lua_State *L = li->luaobject->interpreter->L;
    lua_beginblock(L);
    lua_Object ltable = lua_getref(L, li->luaobject->ref);
    if (!lua_istable(L, ltable)) {
        PyErr_SetString(PyExc_TypeError, "Lua object is not iterable!");
        return NULL;
    }
    PyObject *ret = NULL;
    /* Save key for next iteration. */
    li->refiter = lua_next(L, ltable, li->refiter);
    if (li->refiter > 0) {
        int argn = li->luaobject->indexed ? 2 : 1;
        ret = lua_interpreter_stack_convert(li->luaobject->interpreter, argn);  // value / key
        if (is_object_container(L, lua_getparam(L, argn))) {
            // The object will be shared with the python which in turn will remove a reference.
            // python does not know that the lua is managing the reference.
            Py_INCREF(ret);
        }
    } else {
        /* Raising of standard StopIteration exception with empty value. */
        PyErr_SetNone(PyExc_StopIteration);
    }
    lua_endblock(L);
    return ret;
}

static void LuaObjectIter_dealloc(luaiterobject *li) {
    Py_XDECREF(li->luaobject);
    PyObject_GC_Del(li);
}

PyTypeObject LuaObjectIter_Type = {
    PyVarObject_HEAD_INIT(&LuaObjectIter_Type, 0)
    "LuaObject_iterator",                       /* tp_name */
    sizeof(luaiterobject),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    (destructor) LuaObjectIter_dealloc,         /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    0,                                          /* tp_doc */
    0,                                          /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    PyObject_SelfIter,                          /* tp_iter */
    (iternextfunc) LuaObjectIter_next,           /* tp_iternext */
    0,                                          /* tp_methods */
    0,
};

static PyObject *LuaObjectIter_new(LuaObject *luaobject, PyTypeObject *itertype) {
    luaiterobject *li;
    li = PyObject_GC_New(luaiterobject, itertype);
    if (li == NULL)
        return NULL;
    Py_INCREF(luaobject);
    li->luaobject = luaobject;
    li->refiter = 0;
    return (PyObject *) li;
}

static PyObject *LuaObject_iter(LuaObject *self) {
    return LuaObjectIter_new(self, &LuaObjectIter_Type);
}

static int LuaObject_length(LuaObject *self) {
    lua_beginblock(self->interpreter->L);
    int len = 0;
    lua_Object lobj = lua_getref(self->interpreter->L, self->ref);
    if (lua_isfunction(self->interpreter->L, lobj)) {
        len = 1;  // 1 is True
    } else if (lua_isstring(self->interpreter->L, lobj)) {
        len = lua_strlen(self->interpreter->L, lobj);
    } else if (lua_istable(self->interpreter->L, lobj)) {
        len = lua_tablesize(self->interpreter->L, lobj);
    }
    lua_endblock(self->interpreter->L);
    return len;
}

static PyObject *LuaObject_subscript(LuaObject *self, PyObject *key) {
    return LuaObject_getattr(self, key);
}

static int LuaObject_ass_subscript(LuaObject *self, PyObject *key, PyObject *value) {
    return LuaObject_setattr(self, key, value);
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
    "lua.LuaObject",          /*tp_name*/
    sizeof(LuaObject),        /*tp_basicsize*/
    0,                        /*tp_itemsize*/
    (destructor)LuaObject_dealloc, /*tp_dealloc*/
    0,                        /*tp_print*/
    0,                        /*tp_getattr*/
    0,                        /*tp_setattr*/
    0,                        /*tp_compare*/
    (reprfunc) LuaObject_str, /*tp_repr*/
    0,                        /*tp_as_number*/
    0,                        /*tp_as_sequence*/
    &LuaObject_as_mapping,    /*tp_as_mapping*/
    0,                        /*tp_hash*/
    (ternaryfunc) LuaObject_call, /*tp_call*/
    (reprfunc) LuaObject_str, /*tp_str*/
    (getattrofunc) LuaObject_getattr, /*tp_getattro*/
    (setattrofunc) LuaObject_setattr, /*tp_setattro*/
    0,                        /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "custom lua object",      /*tp_doc*/
    0,                        /*tp_traverse*/
    0,                        /*tp_clear*/
    0,                        /*tp_richcompare*/
    0,                        /*tp_weaklistoffset*/
    (getiterfunc) LuaObject_iter, /*tp_iter*/
    0,                        /*tp_iternext*/
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
    PyObject_Del,             /*tp_free*/
    0,                        /*tp_is_gc*/
};


PyObject *Lua_run(InterpreterObject *self, PyObject *args, int eval) {
    lua_beginblock(self->L);
    PyObject *ret = NULL;
    char *buf = NULL;
    char *s;
    int len;

    if (!PyArg_ParseTuple(args, "s#", &s, &len))
        return NULL;

    if (eval) {
        char *prefix = "return ";
        buf = (char *) malloc(strlen(prefix) + len + 1);
        strcpy(buf, prefix);
        strncat(buf, s, (size_t) len);
        s = buf;
        len = strlen(prefix) + len;
    }
    if (lua_dobuffer(self->L, s, len, "<python>") != 0) {
        char *format = "eval code (%s)";
        char buff[buffsize_calc(2, format, s)];
        sprintf(buff, format, s);
        python_new_error(PyExc_RuntimeError, &buff[0]);
        if (eval) free(buf);
        return NULL;
    }
    if (eval) free(buf);
    int nargs = lua_gettop(self->L);
    if (nargs > 0) {
        ret = lua_interpreter_stack_convert(self, 1);
    }
    if (!ret) {
        Py_INCREF(Py_None);
        ret = Py_None;
    }
    lua_endblock(self->L);
    return ret;
}

PyObject *Interpreter_execute(InterpreterObject *self, PyObject *args) {
    return Lua_run(self, args, 0);
}

PyObject *Interpreter_eval(InterpreterObject *self, PyObject *args) {
    return Lua_run(self, args, 1);
}

PyObject *Interpreter_globals(InterpreterObject *self, PyObject *args) {
    PyObject *ret = NULL;
    lua_Object lobj = lua_getglobal(self->L, "_G");
    if (lua_isnil(self->L, lobj)) {
        PyErr_SetString(PyExc_RuntimeError, "lost globals reference");
        return NULL;
    }
    ret = lua_interpreter_stack_convert(self, 1);
    if (!ret)
        PyErr_Format(PyExc_TypeError, "failed to convert globals table");
    return ret;
}

static PyObject *Interpreter_dofile(InterpreterObject *self, PyObject *args) {
    lua_beginblock(self->L);
    const char *command = NULL;

    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;

    int ret = lua_dofile(self->L, (char *) command);
    if (ret) {
        char *format = "require file (%s)";
        char buff[buffsize_calc(2, format, command)];
        sprintf(buff, format, command);
        python_new_error(PyExc_ImportError, &buff[0]);
        return NULL;
    }
    lua_endblock(self->L);
    return PyInt_FromLong(ret);
}

/*
 * Initialization function environment.
 */
static int Interpreter_init(InterpreterObject *self, PyObject *args, PyObject *kwargs) {
#ifdef CGILUA_ENV
    const char *command = NULL;

    if (!PyArg_ParseTuple(args, "s", &command)) {
        PyErr_SetString(PyExc_TypeError, "enter the path to the directory \"cgilua.conf\"");
        self->L = NULL;
        return -1;
    }
    char *path[1] = {(char *) command};

    self->L = lua_main(1, path);
#else
    self->L = lua_open();

    // default libs
    lua_iolibopen(self->L);
    lua_strlibopen(self->L);
    lua_mathlibopen(self->L);
#endif
    self->isPyType = true;
    luaopen_python(self->L);
    return 0;
};

static void Interpreter_dealloc(InterpreterObject *self) {
    if (self->L) {
        lua_close(self->L);
        self->L = NULL; // lua_State(NULL)
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMethodDef Interpreter_methods[] = {
    {"execute", (PyCFunction) Interpreter_execute, METH_VARARGS,
            "execute arbitrary expressions of the interpreter."},
    {"eval",    (PyCFunction) Interpreter_eval,    METH_VARARGS,
            "evaluates the expression and return its value."},
    {"globals", (PyCFunction) Interpreter_globals, METH_NOARGS,
            "returns the list of global variables."},
    {"require", (PyCFunction) Interpreter_dofile,  METH_VARARGS,
            "loads and executes the script."},
    {NULL,         NULL}
};


static PyObject *lua_get_version(InterpreterObject *self, PyObject *args) {
    return PyString_FromString(LUA_EXT_VERSION);

}

static PyTypeObject InterpreterObject_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "lua.Interpreter",      /*tp_name*/
    sizeof(InterpreterObject), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor) Interpreter_dealloc, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/*tp_flags*/
    "Lua interpreter",         /* tp_doc */
    0,		                  /* tp_traverse */
    0,		                  /* tp_clear */
    0,		                  /* tp_richcompare */
    0,		                  /* tp_weaklistoffset */
    0,		                  /* tp_iter */
    0,		                  /* tp_iternext */
    Interpreter_methods,      /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Interpreter_init,/* tp_init */
    PyType_GenericAlloc,       /* tp_alloc */
    PyType_GenericNew,         /* tp_new */
    PyObject_Del,              /*tp_free*/
    0,                         /*tp_is_gc*/
};

static PyMethodDef lua_methods[] = {
    {"get_version", (PyCFunction) lua_get_version, METH_VARARGS,
            "return version of the lua extension"},
    {NULL, NULL}
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

PyMODINIT_FUNC PyInit_lua(void) {
    PyObject *m;

#if PY_MAJOR_VERSION >= 3
    if (PyType_Ready(&LuaObject_Type) < 0)
        return NULL;
    m = PyModule_Create(&lua_module);
    if (m == NULL) return NULL;
#else
    if (PyType_Ready(&LuaObject_Type) < 0)
        return;

    if (PyType_Ready(&InterpreterObject_Type) < 0)
        return;

    m = Py_InitModule3("lua", lua_methods,
                       "Lunatic-Python Python-Lua bridge");
    if (m == NULL) return;
#endif

    Py_INCREF(&LuaObject_Type);
    Py_INCREF(&InterpreterObject_Type);
    PyModule_AddObject(m, "Interpreter", (PyObject *)&InterpreterObject_Type);

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
