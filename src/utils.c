//
// Created by alex on 28/09/2015.
//

#include <stdarg.h>
#include <string.h>
#include <lua.h>
#include <Python.h>
#include "utils.h"
#include "constants.h"

/* Returns the numeric value stored in API */
int python_getnumber(lua_State *L, char *name) {
    lua_pushobject(L, lua_getglobal(L, PY_API_NAME));
    lua_pushstring(L, name);
    return (int) lua_getnumber(L, lua_rawgettable(L));
}

/* Returns the string value stored in the API */
char *python_getstring(lua_State *L, char *name) {
    lua_pushobject(L, lua_getglobal(L, PY_API_NAME));
    lua_pushstring(L, name);
    return lua_getstring(L, lua_rawgettable(L));
}

/* Stores the value in the given key in the API python */
void python_setstring(lua_State *L, char *name, char *value) {
    set_table_string(L, lua_getglobal(L, PY_API_NAME), name, value);
}

/* Stores the value in the given key in the API python */
void python_setnumber(lua_State *L, char *name, int value) {
    set_table_number(L, lua_getglobal(L, PY_API_NAME), name, value);
}

/**
 * Tag event base
 **/
int python_api_tag(lua_State *L) {
    return python_getnumber(L, PY_API_TAG);
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

void python_new_error(PyObject *exception, char *message) {
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
        PyErr_SetString(exception, message);
        return;
    }
    char *format = "%s\n%s";
    char buff[buffsize_calc(3, format, message, error)];
    sprintf(buff, format, message, error);
    free(error); // free pointer!
    PyErr_SetString(exception, &buff[0]);
}

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
    if (!error) {
        lua_error(L, message);
        return;
    }
    char *format = "%s (%s)";
    char buff[buffsize_calc(3, format, message, error)];
    sprintf(buff, format, message, error);
    free(error); // free pointer!
    lua_error(L, &buff[0]);
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
