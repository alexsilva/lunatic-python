//
// Created by alex on 28/09/2015.
//

#include <stdarg.h>
#include <string.h>
#include <lua.h>
#include <Python.h>
#include "utils.h"

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
