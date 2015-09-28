//
// Created by alex on 28/09/2015.
//

#include <stdarg.h>
#include <string.h>
#include <lua.h>
#include <Python.h>
#include "utils.h"

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

void lua_new_error(lua_State *L, char *message) {
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);

    char *py_error_msg = NULL;

    if (pvalue != NULL) {
        py_error_msg = PyString_AsString(pvalue);
    }
    if (!py_error_msg) {
        lua_error(L, message);
        return;
    }
    char *format = "%s (%s)";
    char buff[calc_buff_size(3, format, message, py_error_msg)];
    sprintf(buff, format, message, py_error_msg);
    lua_error(L, &buff[0]);
}
