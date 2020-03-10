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
#include "lshared.h"
#include "constants.h"

#if defined(_WIN32)
#include "lapi.h"
#endif

#ifdef CGILUA_ENV
#include "cgilua.h"
#else
#include <lualib.h>
#endif


static PyObject *LuaCall(LuaObject *self, lua_Object lobj, PyObject *args) {
    if (!PyTuple_Check(args)) {
        PyErr_SetString(PyExc_TypeError, "tuple expected");
        return NULL;
    }
    PyObject *arg;
    int nargs, index;
    nargs = PyTuple_Size(args);
    for (index = 0; index < nargs; index++) {
        arg = PyTuple_GetItem(args, index); // Borrowed reference.
        if (arg == NULL) {
            PyErr_Format(PyExc_TypeError, "failed to get tuple item #%d", index);
            return NULL;
        }
        switch (py_convert(self->interpreter->L, arg)) {
            case WRAPPED: // The object is being managed by the Lua
                Py_INCREF(arg); // PyTuple_GetItem (Borrowed reference)
                break;
            case CONVERTED:
                break; // nop
            default:
                PyErr_Format(PyExc_TypeError, "failed to convert argument #%d", index);
                return NULL;
        }
    }
    if (lua_callfunction(self->interpreter->L, lobj) != 0 ||
            lua_traceback_checkerror(self->interpreter->L)) {
        char *name;  // get function name
        lua_getobjname(self->interpreter->L, lobj, &name);
        name = name ? name : "?";
        char *format = "call function lua (%s)";
        char buff[buffsize_calc(2, format, name)];
        sprintf(buff, format, name);
        python_new_error(self->interpreter->L, PyExc_RuntimeError, &buff[0]);
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
        for (index = 0; index < nargs; index++) {
            arg = lua_interpreter_stack_convert(self->interpreter, index + 1);
            if (!arg) {
                PyErr_Format(PyExc_TypeError, "failed to convert return #%d", index);
                Py_DECREF(ret);
                return NULL;
            }
            PyTuple_SetItem(ret, index, arg);
        }
    } else if (!PyErr_Occurred()) {
        Py_INCREF(Py_None);
        ret = Py_None;
    } else {
        ret = NULL;
    }
    return ret;
}

static void LuaObject_dealloc(LuaObject *self) {
    if (self->interpreter) { // blocked in init ?
        lua_unref(self->interpreter->L, self->ref);
        if (!self->interpreter->isPyType) {
            self->interpreter->L = NULL;
            free(self->interpreter);
        } else {
            Py_DECREF(self->interpreter);
        }
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *LuaObject_getattr(LuaObject *self, PyObject *attr) {
    lua_beginblock(self->interpreter->L);
    lua_Object ltable = lua_getref(self->interpreter->L, self->ref);
    PyObject *ret = NULL;
    if (lua_isnil(self->interpreter->L, ltable)) {
        PyErr_SetString(PyExc_ValueError, "lost reference");
    } else if (!lua_istable(self->interpreter->L, ltable) &&
               !lua_isuserdata(self->interpreter->L, ltable)) {
        PyErr_SetString(PyExc_AttributeError, "not an indexable value");
    } else {
        lua_pushobject(self->interpreter->L, ltable); // push table
        if (py_convert(self->interpreter->L, attr) != UNCHANGED) { // push key
            lua_Object lobj = lua_gettable(self->interpreter->L);
            ret = lua_interpreter_object_convert(self->interpreter, lobj); // convert
        } else {
            PyErr_SetString(PyExc_ValueError, "can't convert attr/key");
        }
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
    } else if (!lua_istable(self->interpreter->L, ltable) &&
               !lua_isuserdata(self->interpreter->L, ltable)) {
        PyErr_SetString(PyExc_TypeError, "Lua object is not a table");
        return ret;
    }
    lua_pushobject(self->interpreter->L, ltable); // push table
    Conversion res = py_convert(self->interpreter->L, attr);
    if (isvalidstatus(res)) {
        if (value == NULL) {
            lua_pushnil(self->interpreter->L);
            res = CONVERTED;
        } else if ((res = py_convert(self->interpreter->L, value)) == WRAPPED) { // push value ?
            // The object is being managed by the Lua
            // This is a borrowed reference
            Py_INCREF(value);
        }
        if (isvalidstatus(res)) {
            lua_settable(self->interpreter->L);
            self->indexed = is_indexed_array(self->interpreter->L, ltable);
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
            return PyUnicode_FromFormat("<Lua number %ld>", (long) lua_getnumber(self->interpreter->L, lobj));
        case LUA_T_STRING:
            return PyUnicode_FromFormat("<Lua string size %ld>", lua_strlen(self->interpreter->L, lobj));
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
            return PyUnicode_FromFormat("nil");
        default:
            return PyUnicode_FromFormat("invalid type");
    }
    lua_endblock(self->interpreter->L);
    return PyUnicode_FromFormat(buff);
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
    if (lua_isnil(L, ltable)) {
        PyErr_SetString(PyExc_RuntimeError, "lost reference");
        return NULL;
    } else if (!lua_istable(L, ltable) && !lua_isuserdata(L, ltable)) {
        PyErr_SetString(PyExc_TypeError, "Lua object is not iterable!");
        return NULL;
    }
    PyObject *ret = NULL;
    /* Save key for next iteration. */
    li->refiter = lua_next(L, ltable, li->refiter);
    if (li->refiter > 0) {
        int argn = li->luaobject->indexed ? 2 : 1;
        ret = lua_interpreter_stack_convert(li->luaobject->interpreter, argn);  // value / key
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

static PyTypeObject LuaObjectIter_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "LuaObject_iterator",                        /* tp_name */
    .tp_basicsize = sizeof(luaiterobject),                  /* tp_basicsize */
    .tp_itemsize = 0,                                       /* tp_itemsize */
    .tp_dealloc = (destructor) LuaObjectIter_dealloc,       /* tp_dealloc */
    .tp_getattro = PyObject_GenericGetAttr,                 /* tp_getattro */
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    .tp_iter = PyObject_SelfIter,                           /* tp_iter */
    .tp_iternext = (iternextfunc) LuaObjectIter_next,       /* tp_iternext */
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

static int LuaObject_init(LuaObject *self, PyObject *args, PyObject *kwargs) {
    self->interpreter = NULL;
    PyErr_SetString(PyExc_NotImplementedError,
                    "can not be instantiated"
                    " (available only for comparison purposes)");
    return -1;
}

static PyMappingMethods LuaObject_as_mapping = {
    (lenfunc)LuaObject_length,    /*mp_length*/
    (binaryfunc)LuaObject_subscript,/*mp_subscript*/
    (objobjargproc)LuaObject_ass_subscript,/*mp_ass_subscript*/
};

PyTypeObject LuaObject_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "lua.LuaObject",          /*tp_name*/
    .tp_basicsize = sizeof(LuaObject),        /*tp_basicsize*/
    .tp_itemsize = 0,                        /*tp_itemsize*/
    .tp_dealloc = (destructor)LuaObject_dealloc, /*tp_dealloc*/
    .tp_repr = (reprfunc) LuaObject_str, /*tp_repr*/
    .tp_as_mapping = &LuaObject_as_mapping,    /*tp_as_mapping*/
    .tp_call = (ternaryfunc) LuaObject_call, /*tp_call*/
    .tp_str = (reprfunc) LuaObject_str, /*tp_str*/
    .tp_getattro = (getattrofunc) LuaObject_getattr, /*tp_getattro*/
    .tp_setattro = (setattrofunc) LuaObject_setattr, /*tp_setattro*/
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    .tp_doc = "custom lua object",      /*tp_doc*/
    .tp_iter = (getiterfunc) LuaObject_iter, /*tp_iter*/
    .tp_init = (initproc) LuaObject_init,/*tp_init*/
    .tp_alloc = PyType_GenericAlloc,      /*tp_alloc*/
    .tp_new = PyType_GenericNew,        /*tp_new*/
    .tp_free = PyObject_Del,             /*tp_free*/
};


static PyObject *Lua_run(InterpreterObject *self, PyObject *args, int eval) {
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
    if (lua_dobuffer(self->L, s, len, "<python>") != 0 || lua_traceback_checkerror(self->L)) {
        char *format = "eval code (%s)";
        char buff[buffsize_calc(2, format, s)];
        sprintf(buff, format, s);
        python_new_error(self->L, PyExc_RuntimeError, &buff[0]);
        if (eval) free(buf);
        return NULL;
    }
    if (!PyErr_Occurred()) {
        if (eval) free(buf);
        int nargs = lua_gettop(self->L);
        if (nargs > 0) {
            ret = lua_interpreter_stack_convert(self, 1);
        }
        if (!ret) {
            Py_INCREF(Py_None);
            ret = Py_None;
        }
    } else {
        ret = NULL;
    }
    lua_endblock(self->L);
    return ret;
}

/* Function that allows you to add a global variable in the lua interpreter */
static PyObject *Lua_setglobal(InterpreterObject *self, PyObject *args) {
    lua_beginblock(self->L);
    const char *name = NULL;
    PyObject *pyObject = NULL;

    if (!PyArg_ParseTuple(args, "sO", &name, &pyObject))
        return NULL;

    push_pyobject_container(self->L, pyObject, check_pyobject_index(pyObject));
    Py_INCREF(pyObject); // lua ref

    lua_setglobal(self->L, (char *) name);
    lua_endblock(self->L);
    Py_RETURN_NONE;
}


static PyObject *Interpreter_execute(InterpreterObject *self, PyObject *args) {
    return Lua_run(self, args, 0);
}

static PyObject *Interpreter_eval(InterpreterObject *self, PyObject *args) {
    return Lua_run(self, args, 1);
}

static PyObject *Interpreter_globals(InterpreterObject *self, PyObject *args) {
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
    PyErr_Clear(); // clean state
    int ret = lua_dofile(self->L, (char *) command);
    if (ret || lua_traceback_checkerror(self->L) || PyErr_Occurred()) {
        python_new_error(self->L, PyExc_ImportError, (char *) command);
        return NULL;
    }
    lua_endblock(self->L);
    return PyLong_FromLong(ret);
}

/*
 * Initialization function environment.
 */
static int Interpreter_init(InterpreterObject *self, PyObject *args, PyObject *kwargs) {
#ifdef CGILUA_ENV
    const char *argv = NULL;

    if (!PyArg_ParseTuple(args, "s", &argv)) {
        PyErr_SetString(PyExc_TypeError, "enter the path to the directory \"cgilua.conf\"");
        self->L = NULL;
        return -1;
    }

    self->L = lua_setup(argv);
#else
    self->L = lua_open();

    // default libs
    lua_iolibopen(self->L);
    lua_strlibopen(self->L);
    lua_mathlibopen(self->L);
#endif
    if (self->L) {
        PyErr_Clear(); // clean state
        self->isPyType = true;
        int ret = luaopen_python(self->L);
        if (ret == 0) {/* lua is being embedded in python */
            get_python(self->L)->lua->embedded = true;
        } else {
            PyErr_SetString(PyExc_Exception, "python initialization failed");
        }
        return ret;
    } else {
        if (!PyErr_Occurred())  /* If no error was previously configured. */
            PySys_WriteStderr("%s", "startup failed");
        return -1;
    }
};

static void Interpreter_dealloc(InterpreterObject *self) {
    if (self->L) {
        lua_close(self->L);
        self->L = NULL; // lua_State(NULL)
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

#ifdef CGILUA_ENV
static PyObject *Interpreter_unlock_state(InterpreterObject *self) {
    self->L->lockedState = 0;
    return PyLong_FromLong(self->L->lockedState);
}
#endif

static PyMethodDef Interpreter_methods[] = {
    {"execute", (PyCFunction) Interpreter_execute, METH_VARARGS,
            "execute arbitrary expressions of the interpreter."},
    {"eval",    (PyCFunction) Interpreter_eval,    METH_VARARGS,
            "evaluates the expression and return its value."},
    {"setglobal", (PyCFunction) Lua_setglobal,    METH_VARARGS,
            "add a global value in the interpreter state."},
    {"globals", (PyCFunction) Interpreter_globals, METH_NOARGS,
            "returns the list of global variables."},
    {"require", (PyCFunction) Interpreter_dofile,  METH_VARARGS,
            "loads and executes the script."},
#ifdef CGILUA_ENV
    {"unlock_state", (PyCFunction) Interpreter_unlock_state,  METH_VARARGS,
            "unlocks the interpreter state"},
#endif
    {NULL,         NULL}
};


static PyObject *lua_get_version(InterpreterObject *self, PyObject *args) {
    return PyBytes_FromString(LUA_EXT_VERSION);
}

static PyTypeObject InterpreterObject_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "lua.Interpreter",      /*tp_name*/
    .tp_basicsize = sizeof(InterpreterObject), /*tp_basicsize*/
    .tp_itemsize = 0,                         /*tp_itemsize*/
    .tp_dealloc =(destructor) Interpreter_dealloc, /*tp_dealloc*/
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,/*tp_flags*/
    .tp_doc = "Lua interpreter",         /* tp_doc */
    .tp_methods = Interpreter_methods,      /* tp_methods */
    .tp_init = (initproc)Interpreter_init,/* tp_init */
    .tp_alloc = PyType_GenericAlloc,       /* tp_alloc */
    .tp_new = PyType_GenericNew,         /* tp_new */
    .tp_free = PyObject_Del,              /*tp_free*/
};

static PyMethodDef lua_methods[] = {
    {"get_version", (PyCFunction) lua_get_version, METH_VARARGS,
            "return version of the lua extension"},
    {NULL, NULL}
};

static struct PyModuleDef lua_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "lua",
    .m_doc = "Lunatic-Python Python-Lua bridge",
    .m_methods = lua_methods,
    .m_size = -1,
};

PyMODINIT_FUNC PyInit_lua(void) {
    if (PyType_Ready(&LuaObject_Type) < 0)
        return NULL;

    if (PyType_Ready(&InterpreterObject_Type) < 0)
        return NULL;

    if (PyType_Ready(&LuaObjectIter_Type) < 0)
        return NULL;

    PyObject *m = PyModule_Create(&lua_module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&LuaObject_Type);
    Py_INCREF(&InterpreterObject_Type);

    PyModule_AddObject(m, "Interpreter", (PyObject *)&InterpreterObject_Type);
    PyModule_AddObject(m, "LuaObject", (PyObject *)&LuaObject_Type);

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
