#include "fileio.h"
#include <Python.h>
#include "../pyconv.h"
#include "../utils.h"

#define FOUTPUT  "_OUTPUT"
#define FINPUT   "_INPUT"
#define FSTDOUT  "_STDOUT"
#define FSTDIN   "_STDIN"

#define MAXPRINT 50


/* Open a new python file */
static PyObject *openfile(lua_State *L, char *filename, char *mode) {
    PyObject *file = PyFile_FromString(filename, mode);
    if (!file) {
        lua_pushnil(L);
        lua_pushstring(L, "failed to open file");
        PyErr_Clear();   // clear error stack
    }
    return file;
}

/* Open a new python file */
void io_openfile(lua_State *L) {
    char *filename = luaL_check_string(L, 1);
    char *mode = luaL_opt_string(L, 2, "r");
    PyObject *file = openfile(L, filename, mode);
    if (file) {
        push_pyobject_container(L, file, false);
    }
}

static PyObject *closefile(lua_State *L, PyObject *file) {
    if (file) {
        PyObject *py_file_close = PyString_FromString("close");
        if (py_file_close) {
            PyObject *value = PyObject_CallMethodObjArgs(file, py_file_close, NULL);
            Py_DECREF(py_file_close);
            return value;
        }
    }
    return NULL;
}

static void setglogalfile(lua_State *L, char *name, PyObject *file) {
    push_pyobject_container(L, file, false);
    lua_rawgetglobal(L, name);
}

static PyObject *getfilebyname(lua_State *L, char *name) {
    return get_pobject(L, lua_rawgetglobal(L, name));
}


static void io_closefile(lua_State *L) {
    lua_Object lobj = lua_getparam(L, 1);
    if (is_object_container(L, lobj)) {
        Py_XDECREF(closefile(L, get_pobject(L, lobj)));
        if (PyErr_Occurred()) { PyErr_Clear(); }
    }
}


static void io_writeto(lua_State *L) {
    lua_Object f = lua_getparam(L, 1);

    if (f == LUA_NOOBJECT) {
        Py_XDECREF(closefile(L, getfilebyname(L, FOUTPUT))); // FOUTPUT.close
        if (PyErr_Occurred()) { PyErr_Clear(); }

        PyObject *file = getfilebyname(L, FSTDOUT);
        setglogalfile(L, FOUTPUT, file);  // FOUTPUT = FSTDOUT

        push_pyobject_container(L, file, false);

    } else if (is_object_container(L, f)) {
        push_pyobject_container(L, get_pobject(L, f), false);
    } else {
        //  opens and returns the new file
        PyObject *pyfile = openfile(L, luaL_check_string(L, 1), "w");
        if (!pyfile) {
            setglogalfile(L, FOUTPUT, pyfile);
        }
    }
}

/* Write to a python file */
static void io_write(lua_State *L) {
    int arg = 1;
    lua_Object lfile = lua_getparam(L, arg);
    PyObject *file;

    if (is_object_container(L, lfile)) {
        file = get_pobject(L, lfile);
        if (!PyFile_Check(file)) {
            file = getfilebyname(L, FOUTPUT);
        } else {
            arg++;  // 2
        }
    } else {
        file = getfilebyname(L, FOUTPUT);
    }

    PyObject *py_file_write = PyString_FromString("write");
    PyObject *value = NULL, *py_object = NULL;
    lua_Object lua_object;

    while ((lua_object = lua_getparam(L, arg++)) != LUA_NOOBJECT) {
        py_object = lua_object_convert(L, lua_object);
        value = PyObject_CallMethodObjArgs(file, py_file_write, py_object, NULL);
        Py_DECREF(py_object);
        if (!value) {
            lua_pushnil(L);
            lua_pushstring(L, "write failed!");
            break;
        } else {
            Py_DECREF(value);
        }
    }
    Py_DECREF(py_file_write);
    lua_pushuserdata(L, NULL);
}


static void write_print(lua_State *L, PyObject *file, char *s) {
    PyObject *py_file_write = PyString_FromString("write");
    PyObject *value = NULL, *py_object = NULL;
    py_object = PyString_FromString(s);
    value = PyObject_CallMethodObjArgs(file, py_file_write, py_object, NULL);
    Py_DECREF(py_object);
    if (!value) {
        lua_pushnil(L);
        lua_pushstring(L, "write failed!");
    } else {
        Py_DECREF(value);
    }
    Py_DECREF(py_file_write);
}

static void io_print(lua_State *L) {
    lua_Object args[MAXPRINT];
    lua_Object obj;
    int number = 0;
    int index;
    while ((obj = lua_getparam(L, number+1)) != LUA_NOOBJECT) {
        luaL_arg_check(L, number < MAXPRINT, number+1, "too many arguments");
        args[number++] = obj;
    }
    PyObject *file = getfilebyname(L, FOUTPUT);
    for (index=0; index<number; index++) {
        lua_pushobject(L, args[index]);
        if (lua_call(L, "tostring"))
            lua_error(L, "error in `tostring' called by `print'");
        obj = lua_getresult(L, 1);
        if (!lua_isstring(L, obj))
            lua_error(L, "`tostring' must return a string to `print'");
        if (index > 0) write_print(L, file, "\t");
        write_print(L, file, lua_getstring(L, obj));
    }
    write_print(L, file, "\n");
}


struct luaL_reg file_io_function[] = {
        {"openfile",  io_openfile},
        {"writeto",   io_writeto},
        {"closefile", io_closefile},
        {"write",     io_write},
        {"print",     io_print},
        {NULL, NULL}
};


/* Register the new IO interface */
void fileio_register(lua_State *L) {
    Python *python = get_python(L);
    if (!python->io && python->lua->embedded) {
        python->io = true;
        int index = 0;
        while (file_io_function[index].name) {
            lua_pushcfunction(L, file_io_function[index].func);
            lua_setglobal(L, file_io_function[index].name);
            index++;
        }
    }
}