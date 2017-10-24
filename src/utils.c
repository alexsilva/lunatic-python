//
// Created by alex on 28/09/2015.
//
extern "C"
{
#include <lua.h>
#include <Python.h>
}

#include <cgilua.h>
#include "utils.h"
#include "constants.h"

Python::Python(lua_State *L) : lua(L) {
    object_ref = false;
    embedded = false;
    stack  = nullptr;
}

Python *get_python(lua_State *L) {
    auto *python = (Python *) lua_getuserdata(L, lua_getglobal(L, PY_API_NAME));
    if (!python) lua_new_error(L, ptrchar "python ref not found!");
    return python;
}

/**
 * Tag event base
 **/
int python_api_tag(lua_State *L) {
    return get_python(L)->lua.get_tag();
}

/* Returns the number of elements in a table */
int lua_tablesize(lua_State *L, lua_Object ltable) {
    lua_pushobject(L, ltable);
    lua_call(L, ptrchar "getn");
    return (int) lua_getnumber(L, lua_getresult(L, 1));
}

#ifndef strdup
char *strdup(const char * s) {
    size_t len = strlen(s) + 1;
    auto *p = (char *) malloc(len);
    return static_cast<char *>(p ? memcpy(p, s, len) : nullptr);
}
#endif

static char *tostring(PyObject *obj) {
    PyObject *pObjStr = PyObject_Str(obj);
    char *str;
    if (pObjStr) {
        str = PyString_AsString(pObjStr);
        if (str && strlen(str) == 0) {
            Py_DECREF(pObjStr);
            return nullptr;
        }
        str = strdup(str);
        Py_DECREF(pObjStr);
    } else {
        PyErr_Clear();
        str = nullptr;
    }
    return str;
}

#define ATTRNAME "__name__"

char *get_pyobject_str(PyObject *obj) {
    if (!obj) return nullptr;
    char *str;
    if (PyCallable_Check(obj) && PyObject_HasAttrString(obj, ATTRNAME)) {
        PyObject *pObjStr = PyObject_GetAttrString(obj, ATTRNAME);  // function name
        if (!pObjStr) {
            PyErr_Clear();
            return nullptr;
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


char *python_error_message() {
    char *msg = nullptr;
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    if (pvalue || ptype || ptraceback) {
        msg = get_pyobject_str(pvalue);
        if (!msg) {
            msg = get_pyobject_str(ptype);
            if (!msg)
                msg = get_pyobject_str(ptraceback);
        }
        Py_XDECREF(ptype);
        Py_XDECREF(pvalue);
        Py_XDECREF(ptraceback);
    }
    return msg;
}

/* lua inside python (interface python) */
void python_new_error(PyObject *exception, char *message) {
    char *error = PyErr_Occurred() ? python_error_message() : nullptr;
    if (error) {
        const char *format = "%s\n%s";
        char buff[buffsize_calc(3, format, message, error)];
        sprintf(buff, format, message, error);
        free(error); // free pointer!
        PyErr_SetString(exception, &buff[0]);
    } else {
        PyErr_SetString(exception, message);
    }
}

/* lua inside python (access python objects) */
static void lua_virtual_error(lua_State *L, char *message) {
    python_new_error(PyExc_Exception, message);
    lua_errorfallback(L, message);
    lua_call(L, ptrchar "lockstate"); // stop operations
    throw EXIT_FAILURE;
}

static void call_lua_error(lua_State *L, char *message) {
    if (get_python(L)->lua.embedded) {
        lua_virtual_error(L, message);
    } else {
        lua_error(L, message);
    }
}

void lua_new_argerror (lua_State *L, int numarg, char *extramsg) {
    lua_Function f = lua_stackedfunction(L, 0);
    char *funcname;
    lua_getobjname(L, f, &funcname);
    numarg -= lua_nups(L, f);
    if (funcname == nullptr)
        funcname = ptrchar "?";
    char buff[500];
    if (extramsg == nullptr)
        sprintf(buff, "bad argument #%d to function `%.50s'",
                numarg, funcname);
    else
        sprintf(buff, "bad argument #%d to function `%.50s' (%.100s)",
                numarg, funcname, extramsg);
    lua_new_error(L, buff);
}

/* python inside lua */
void lua_new_error(lua_State *L, char *message) {
    char *error = PyErr_Occurred() ? python_error_message() : nullptr;
    if (error) {
        const char *format = "%s (%s)";
        char buff[buffsize_calc(3, format, message, error)];
        sprintf(buff, format, message, error);
        free(error); // free pointer!
        call_lua_error(L, &buff[0]);
    } else {
        call_lua_error(L, message);
    }
}

/**
 * Same as 'lua_new_error', but receives a message format.
**/
void lua_raise_error(lua_State *L, char *format,
                     PyObject *obj) {
    char *pstr = get_pyobject_str(obj);
    auto *str = const_cast<char *>(pstr ? pstr : "?");
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
