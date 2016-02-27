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

char *get_pyobject_str(PyObject *pyobject, char *dftstr) {
    char *str = NULL;
    char *attr_name = "__name__";
    if (PyCallable_Check(pyobject) && PyObject_HasAttrString(pyobject, attr_name)) {
        pyobject = PyObject_GetAttrString(pyobject, attr_name);  // function name
    }
    pyobject = PyObject_Str(pyobject);
    if (pyobject) {
        str = PyString_AsString(pyobject);
    }
    return str && strlen(str) > 0 ? str : dftstr;
}

int calc_buff_size(int nargs, ...) {
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
    if (pvalue != NULL) {
        error = get_pyobject_str(pvalue, get_pyobject_str(ptype, NULL));
    }
    if (!error) {
        PyErr_SetString(exception, message);
        return;
    }
    char *format = "%s\n%s";
    char buff[calc_buff_size(3, format, message, error)];
    sprintf(buff, format, message, error);
    PyErr_SetString(exception, &buff[0]);
}

void lua_new_error(lua_State *L, char *message) {
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    char *error = NULL;
    if (pvalue != NULL) {
        error = get_pyobject_str(pvalue, get_pyobject_str(ptype, NULL));
    }
    if (!error) {
        lua_error(L, message);
        return;
    }
    char *format = "%s (%s)";
    char buff[calc_buff_size(3, format, message, error)];
    sprintf(buff, format, message, error);
    lua_error(L, &buff[0]);
}
