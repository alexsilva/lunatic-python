//
// Created by alex on 28/09/2015.
//
#include <lua.h>
#include <Python.h>
#include "utils.h"
#include "constants.h"
#include <string>
#include <frameobject.h>
#include <sstream>

Python::Python(lua_State *L) : lua(L) {
    unicode = new PyUnicode;
    object_ref = false;
    embedded = false;
    stack  = nullptr;
}

Python::~Python() {
    delete unicode;
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

namespace patch {
    template<typename T>
    std::string to_string(const T &n) {
        std::ostringstream stm;
        stm << n;
        return stm.str();
    }
}

std::string get_line_separator(lua_State *L) {
    return get_python(L)->lua.embedded ? "\n" : "<br>";
}

void python_traceback_append(lua_State *L, std::string *stack,
                             PyTracebackObject *traceback) {
    if (traceback != nullptr && traceback->tb_frame != nullptr) {
        PyFrameObject *frame = traceback->tb_frame;
        const char *format = nullptr;
        const char *nline = get_line_separator(L).c_str();

        stack->append("Python stack trace:");
        stack->append(nline);

        while (frame != nullptr) {
            int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
            const char *filename = PyString_AsString(frame->f_code->co_filename);
            const char *funcname = PyString_AsString(frame->f_code->co_name);
            std::string sline = patch::to_string(line);
            const char *cline = sline.c_str();  // ungle

            format = "    %s(%s): %s%s";
            char buff[buffsize_calc(5, format, filename, cline, funcname, nline)];
            sprintf(buff, format, filename, cline, funcname, nline);

            stack->append(&buff[0]);
            frame = frame->f_back;
        }
    }
}

void python_error_message(lua_State *L, std::string *msg) {
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
    if (pvalue || ptype || ptraceback) {
        const char *name;
        name = get_pyobject_str(ptype);
        if (name != nullptr) {
            msg->append(name);
            free((void *) name);
        }
        name = get_pyobject_str(pvalue);
        if (name != nullptr) {
            msg->append(": ");
            msg->append(name);
            msg->append(get_line_separator(L));
            free((void *) name);
        }
        python_traceback_append(L, msg, reinterpret_cast<PyTracebackObject *>(ptraceback));
        Py_XDECREF(ptype);
        Py_XDECREF(pvalue);
        Py_XDECREF(ptraceback);
    }
}

/* lua inside python (interface python) */
void python_new_error(lua_State *L, PyObject *exception, char *message) {
    std::string py_error;

    if (PyErr_Occurred()) python_error_message(L, &py_error);

    bool lua_embedded = get_python(L)->lua.embedded;
    std::string stack_info;

    if (!py_error.empty()) {
        if (lua_embedded) {
            stack_info.append(message);
            stack_info.append(get_line_separator(L));
            stack_info.append(py_error);
            lua_traceback_insert(L, 0, stack_info.c_str());
            PyErr_SetString(exception, lua_traceback_value(L));
        } else {
            stack_info.append(message);
            stack_info.append(get_line_separator(L));
            stack_info.append(py_error);
            PyErr_SetString(exception, stack_info.c_str());
        }
    } else {
        if (lua_embedded) {
            stack_info.append(message);
            stack_info.append(get_line_separator(L));
            lua_traceback_insert(L, 0, stack_info.c_str());
            PyErr_SetString(exception, lua_traceback_value(L));
        } else {
            PyErr_SetString(exception, message);
        }
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
    std::string py_error;
    if (PyErr_Occurred()) python_error_message(L, &py_error);
    if (!py_error.empty()) {
        std::string stack_info(message);
        stack_info.append(get_line_separator(L));
        stack_info.append(py_error);
        lua_error(L, const_cast<char *>(stack_info.c_str()));
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
