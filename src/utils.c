//
// Created by alex on 28/09/2015.
//

#include <stdarg.h>
#include <string.h>
#include <lua.h>
#include <Python.h>
#include <frameobject.h>
#include "utils.h"
#include "constants.h"
#include "stack.h"
#include "lexception.h"
#include "../deps/vstring/vstring.h"

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
    return unicode;
}

Lua *lua_init(lua_State *L) {
    Lua *lua = malloc(sizeof(Lua));
    if (!lua) return NULL;
    lua->tag = lua_newtag(L);
    lua_pushobject(L, lua_createtable(L));
    lua->tableref = lua_ref(L, 1); /* table ref */
    lua->stringbyref = false;
    lua->numberbyref = false;
    lua->embedded = false;
    lua->tableconvert = false;
    return lua;
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
    python->embedded = false;
    python->lua = lua_init(L);
    python->unicode = NULL;
    if (!python->lua) {
        python_free(L, python);
        return NULL;
    }
    python->unicode = python_unicode_init(L);
    if (!python->unicode) {
        python_free(L, python);
        return NULL;
    }
    return python;
}

Python *get_python(lua_State *L) {
    Python *python = lua_getuserdata(L, lua_getglobal(L, PY_API_NAME));
    if (!python) lua_new_error(L, "lost python userdata!");
    return python;
}

void python_free(lua_State *L, Python *python) {
    if (python->lua != NULL) {
        lua_unref(L, python->lua->tableref);
        free(python->lua);
    }
    if (python->unicode != NULL) {
        free(python->unicode);
    }
    free(python);
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

static char *get_line_separator(lua_State *L) {
    return "\n";
}

static void python_traceback_append(lua_State *L, vstring *vs,
                             PyTracebackObject *traceback) {
    if (traceback != NULL && traceback->tb_frame != NULL) {
        PyFrameObject *frame = traceback->tb_frame;
        char *ls = get_line_separator(L);
        char *strace = "Stack trace:";
        char *sfile = "File: \"";
        char *sline = "\", line ";
        char *smodule = ", in ";
        char *sspaces = "  ";

        vs_pushstr(vs, strace, strlen(strace));
        vs_pushstr(vs, ls, strlen(ls));

        while (true) {
            int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
            const char *filename = PyString_AsString(frame->f_code->co_filename);
            const char *funcname = PyString_AsString(frame->f_code->co_name);
            vs_pushstr(vs, sspaces, strlen(sspaces));
            vs_pushstr(vs, sfile, strlen(sfile));
            vs_pushstr(vs, filename, strlen(filename));
            vs_pushstr(vs, sline, strlen(sline));
            vs_pushint(vs, line);
            vs_pushstr(vs, smodule, strlen(smodule));
            vs_pushstr(vs, funcname, strlen(funcname));
            frame = frame->f_back;
            if (frame == NULL)
                break;
            vs_pushstr(vs, ls, strlen(ls));
        }
    }
}

static void python_traceback_message(lua_State *L, vstring *traceback) {
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
    if (pvalue || ptype || ptraceback) {
        const char *name;
        name = get_pyobject_str(ptype);
        if (name != NULL) {
            vs_pushstr(traceback, name, strlen(name));
            free((void *) name);
        }
        name = get_pyobject_str(pvalue);
        if (name != NULL) {
            char *s = ": ";
            char *ls = get_line_separator(L);
            vs_pushstr(traceback, s, strlen(s));
            vs_pushstr(traceback, name, strlen(name));
            vs_pushstr(traceback, ls, strlen(ls));

            free((void *) name);
        }
        python_traceback_append(L, traceback, (PyTracebackObject *) ptraceback);
        Py_XDECREF(ptype);
        Py_XDECREF(pvalue);
        Py_XDECREF(ptraceback);
    }
}

static void PyErr_SetVString(PyObject *exception, vstring *vs) {
    PyErr_SetObject(exception, PyString_FromStringAndSize(vs_contents(vs),
                                                          (Py_ssize_t) vs_len(vs)));
}

/* lua inside python (interface python) */
void python_new_error(lua_State *L, PyObject *exception, char *message) {
    size_t buffsize = 32 * 1024;
    vstring *vstraceback, *vsmessage;

    vstraceback = NULL;
    vstraceback = vs_init(vstraceback, NULL, VS_TYPE_DYNAMIC, NULL, buffsize);
    if (PyErr_Occurred()) python_traceback_message(L, vstraceback);

    vsmessage = NULL;
    vsmessage = vs_init(vsmessage, NULL, VS_TYPE_DYNAMIC, NULL, buffsize);

    char *ls = get_line_separator(L);

    if (vs_len(vstraceback) > 0) {
        vs_pushstr(vsmessage, message, strlen(message));
        vs_pushstr(vsmessage, ls, strlen(ls));
        vs_pushstr(vsmessage, vs_contents(vstraceback), vs_len(vstraceback));
        if (get_python(L)->lua->embedded) {
            const char *ltraceback = lua_traceback_message(L);
            if (ltraceback != NULL) {
                if (strlen(ltraceback) > 0) {
                    vs_pushstr(vsmessage, ltraceback, strlen(ltraceback));
                }
                free((void *) ltraceback);
            }
        }
        PyErr_SetVString(exception, vsmessage);
    } else if (get_python(L)->lua->embedded) {
        vs_pushstr(vsmessage, message, strlen(message));
        vs_pushstr(vsmessage, ls, strlen(ls));
        const char *ltraceback = lua_traceback_message(L);
        if (ltraceback != NULL) {
            if (strlen(ltraceback) > 0) {
                vs_pushstr(vsmessage, ltraceback, strlen(ltraceback));
            }
            free((void *) ltraceback);
        }
        PyErr_SetVString(exception, vsmessage);
    } else {
        PyErr_SetString(exception, message);
    }
    vs_deinit(vstraceback);
    vs_deinit(vsmessage);
}

/* python inside lua */
void lua_new_error(lua_State *L, char *message) {
    size_t buffsize = 32 * 1024;
    vstring *vstraceback = NULL;
    vstraceback = vs_init(vstraceback, NULL, VS_TYPE_DYNAMIC, NULL, buffsize);

    if (PyErr_Occurred()) python_traceback_message(L, vstraceback);

    if (vs_len(vstraceback) > 0) {
        char *ls = get_line_separator(L);

        vstring *vsmessage = NULL;
        vsmessage = vs_init(vsmessage, NULL, VS_TYPE_DYNAMIC, NULL, buffsize);

        vs_pushstr(vsmessage, message, strlen(message));
        vs_pushstr(vsmessage, ls, strlen(ls));
        vs_pushstr(vsmessage, vs_contents(vstraceback), vs_len(vstraceback));

        char *contents = vs_contents(vsmessage);
        size_t ctsize = (size_t) vs_len(vsmessage);
        char lmessage[ctsize + 1];

        memset(lmessage, 0, ctsize + 1);
        strncpy(lmessage, contents, ctsize);

        vs_deinit(vstraceback);
        vs_deinit(vsmessage);

        lua_error(L, lmessage);
    } else {
        vs_deinit(vstraceback);
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
