//
// Created by alex on 28/09/2015.
//

#include <stdarg.h>
#include <string.h>
#include <lua.h>
#include <Python.h>
#include "utils.h"
#include "constants.h"
#include "stack.h"
#include "lexception.h"

void python_unicode_set_encoding(PythonUnicode *unicode, char *encoding) {
    strncpy(unicode->encoding, encoding, PY_UNICODE_MAX);
}

void python_unicode_set_errorhandler(PythonUnicode *unicode, char *errorhandler) {
    strncpy(unicode->errorhandler, errorhandler, PY_UNICODE_MAX);
}

PythonUnicode *python_unicode_init(lua_State *L) {
    PythonUnicode *unicode = malloc(sizeof(PythonUnicode));
    if (!unicode) return NULL;
    python_unicode_set_errorhandler(unicode, "strict");
    python_unicode_set_encoding(unicode, "UTF-8");
}

Lua *lua_init(lua_State *L) {
    Lua *lua = malloc(sizeof(Lua));
    if (!lua) return NULL;
    lua->tag = lua_newtag(L);
    lua_pushobject(L, lua_createtable(L));
    lua->tableref = lua_ref(L, 1); /* table ref */
    lua->byref = false;
    lua->embedded = false;
    lua->tableconvert = false;
}

/* table with attached attributes */
lua_Object get_lua_bindtable(lua_State *L, Lua *lua) {
    lua_Object lua_object = lua_getref(L, lua->tableref);
    if (!lua_istable(L, lua_object)) {
        char *msg = "lost table reference!";
        if (lua->embedded) {
            python_new_error(L, PyExc_LookupError, msg);
        } else {
            lua_new_error(L, msg);
        }
    }
    return lua_object;
}

/* Python objects as apis functions/attributes */
void set_lua_api(lua_State *L, Lua *lua, char *name, void *udata) {
    lua_pushobject(L, get_lua_bindtable(L, lua));
    lua_pushstring(L, name);
    lua_pushusertag(L, udata, lua->tag);
    lua_rawsettable(L);
}

/* Functions (C) of api python */
void set_python_api(lua_State *L, Python *python, char *name, lua_CFunction cfn) {
    lua_pushobject(L, get_lua_bindtable(L, python->lua));
    lua_pushstring(L, name);
    lua_pushcclosure(L, cfn, 0);
    lua_rawsettable(L);
}

Python *python_init(lua_State *L) {
    Python *python = malloc(sizeof(Python));
    if (!python) return NULL;
    Lua *lua = lua_init(L);
    if (!lua) {
        python_free(L, python);
        return NULL;
    }
    PythonUnicode *unicode = python_unicode_init(L);
    if (!unicode) {
        python_free(L, python);
        return NULL;
    }
    python->embedded = false;
    python->unicode = unicode;
    python->lua = lua;
}

Python *get_python(lua_State *L) {
    Python *python = lua_getuserdata(L, lua_getglobal(L, PY_API_NAME));
    if (!python) lua_new_error(L, "lost python userdata!");
    return python;
}

void python_free(lua_State *L, Python *python) {
    free(python);
    free(python->unicode);
    free(python->lua);
}

/**
 * Tag event base
 **/
int python_api_tag(lua_State *L) {
    return get_python(L)->lua->tag;
}

/* Returns the number of elements in a table */
int lua_tablesize(lua_State *L, lua_Object ltable) {
    lua_pushobject(L, ltable);
    lua_call(L, "getn");
    return (int) lua_getnumber(L, lua_getresult(L, 1));
}

#ifndef strdup
char *strdup(const char * s) {
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    return p ? memcpy(p, s, len) : NULL;
}
#endif

static char *tostring(PyObject *obj) {
    PyObject *pObjStr = PyObject_Str(obj);
    char *str;
    if (pObjStr) {
        str = PyString_AsString(pObjStr);
        if (str && strlen(str) == 0) {
            Py_DECREF(pObjStr);
            return NULL;
        }
        str = strdup(str);
        Py_DECREF(pObjStr);
    } else {
        PyErr_Clear();
        str = NULL;
    }
    return str;
}

#define ATTRNAME "__name__"

char *get_pyobject_str(PyObject *obj) {
    if (!obj) return NULL;
    char *str;
    if (PyCallable_Check(obj) && PyObject_HasAttrString(obj, ATTRNAME)) {
        PyObject *pObjStr = PyObject_GetAttrString(obj, ATTRNAME);  // function name
        if (!pObjStr) {
            PyErr_Clear();
            return NULL;
        }
        str = tostring(pObjStr);
        Py_DECREF(pObjStr);
    } else {
        str  = tostring(obj);
    }
    return str;
}

/**
 * Returns the total number of bytes +1 in argument strings.
**/
int buffsize_calc(int nargs, ...) {
    int size = 0;
    va_list ap;
    int i;
    va_start(ap, nargs);
    for(i = 0; i < nargs; i++) {
        size += strlen(va_arg(ap, char *));
    }
    va_end(ap);
    return size + 1;
}

/* lua inside python (interface python) */
void python_new_error(lua_State *L, PyObject *exception, char *message) {
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    char *error = NULL;
    if (pvalue || ptype || ptraceback) {
        error = get_pyobject_str(pvalue);
        if (!error) {
            error = get_pyobject_str(ptype);
            if (!error) error = get_pyobject_str(ptraceback);
        }
        Py_XDECREF(ptype);
        Py_XDECREF(pvalue);
        Py_XDECREF(ptraceback);
    }
    if (!error) {
        lua_error_fallback(L, exception, message);
        //PyErr_SetString(exception, message);
        return;
    }
    char *format = "%s\n%s";
    char buff[buffsize_calc(3, format, message, error)];
    sprintf(buff, format, message, error);
    free(error); // free pointer!
    //PyErr_SetString(exception, &buff[0]);
    lua_error_fallback(L, exception, &buff[0]);
}

/* python inside lua */
void lua_new_error(lua_State *L, char *message) {
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    char *error = NULL;
    if (pvalue || ptype || ptraceback) {
        error = get_pyobject_str(pvalue);
        if (!error) {
            error = get_pyobject_str(ptype);
            if (!error) error = get_pyobject_str(ptraceback);
        }
        Py_XDECREF(ptype);
        Py_XDECREF(pvalue);
        Py_XDECREF(ptraceback);
    }
    if (error) {
        char *format = "%s (%s)";
        char buff[buffsize_calc(3, format, message, error)];
        sprintf(buff, format, message, error);
        free(error); // free pointer!
        lua_error(L, &buff[0]);
    } else {
        lua_error(L, message);
    }
}

/**
 * Same as 'lua_new_error', but receives a message format.
**/
void lua_raise_error(lua_State *L, char *format,
                     PyObject *obj) {
    char *pstr = get_pyobject_str(obj);
    char *str = pstr ? pstr : "?";
    char buff[buffsize_calc(2, format, str)];
    sprintf(buff, format, str);
    free(pstr); // free pointer!
    lua_new_error(L, &buff[0]);
}

/* If the object is an instance of a list */
int PyObject_IsListInstance(PyObject *obj) {
    return PyObject_IsInstance(obj, (PyObject*) &PyList_Type);
}

/* If the object is an instance of a tuple */
int PyObject_IsTupleInstance(PyObject *obj) {
    return PyObject_IsInstance(obj, (PyObject*) &PyTuple_Type);
}

/* If the object is an instance of a dictionary */
int PyObject_IsDictInstance(PyObject *obj) {
    return PyObject_IsInstance(obj, (PyObject*) &PyDict_Type);
}
