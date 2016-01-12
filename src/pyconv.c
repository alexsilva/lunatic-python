//
// Created by alex on 26/09/2015.
//

#include "pyconv.h"
#include "luaconv.h"
#include "utils.h"
#include "constants.h"

/* python string bytes */
static char *get_pyobject_as_string(lua_State *L, PyObject *o) {
    char *s = PyString_AsString(o);
    if (!s) {
        lua_new_error(L, "converting python string");
    }
    return s;
}

/* python string unicode */
static char *get_pyobject_as_utf8string(lua_State *L, PyObject *o) {
    PyObject *obj = PyUnicode_AsUTF8String(o);
    if (!obj) {
        lua_new_error(L, "converting unicode string");
    }
    char *str = get_pyobject_as_string(L, obj);
    Py_DECREF(obj);
    return str;
}

/**/
lua_Object py_object_wrapped(lua_State *L, PyObject *pobj, int asindx) {
    lua_Object ltable = lua_createtable(L);

    set_table_userdata(L, ltable, PY_OBJECT, pobj);
    set_table_number(L, ltable, PY_OBJECT_AS_INDEX, asindx);
    set_table_userdata(L, ltable, LUA_BASE_TAG, 0);  // derived

    // register all tag methods
    int tag = get_base_tag(L);
    lua_pushobject(L, ltable);
    lua_settag(L, tag);

    return ltable;
}

Conversion py_object_wrap_lua(lua_State *L, PyObject *pobj, int asindx) {
    lua_pushobject(L, py_object_wrapped(L, pobj, asindx)); // returning table
    return WRAP;
}

lua_Object _lua_object_raw(lua_State *L, PyObject *obj, lua_Object lptable, PyObject *lpkey) {
    lua_Object ltable = lua_createtable(L);
    if (PyDict_Check(obj)) {
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(obj, &pos, &key, &value)) {
            if (PyDict_Check(value) || PyList_Check(value) || PyTuple_Check(value)) {
                _lua_object_raw(L, value, ltable, key);
            } else {
                lua_pushobject(L, ltable);
                py_convert(L, key);
                py_convert(L, value);
                lua_settable(L);
            }
        }
    } else if (PyList_Check(obj) || PyTuple_Check(obj)) {
        Py_ssize_t size = PyList_Check(obj) ? PyList_Size(obj) : PyTuple_Size(obj);
        Py_ssize_t index;
        PyObject *value;
        for (index = 0; index < size; index++) {
            value = PyObject_GetItem(obj, PyInt_FromSsize_t(index));
            if (PyDict_Check(value) || PyList_Check(value) || PyTuple_Check(value)) {
                _lua_object_raw(L, value, ltable, PyInt_FromSsize_t(index + 1));
            } else {
                lua_pushobject(L, ltable);
                py_convert(L, PyInt_FromSsize_t(index + 1));
                py_convert(L, value);
                lua_settable(L);
            }
        }
    } else {
        lua_error(L, "unsupported raw type");
    }
    if (lptable && lpkey) {
        lua_pushobject(L, lptable);
        py_convert(L, lpkey);
        lua_pushobject(L, ltable);
        lua_settable(L);
    }
    return ltable;
}

void lua_raw(lua_State *L) {
    lua_Object lobj = lua_getparam(L, 1);
    if (is_wrapped_object(L, lobj)) {
        py_object *obj = get_py_object_stack(L, 1);
        lua_pushobject(L, _lua_object_raw(L, obj->o, 0, NULL));
        free(obj);
    } else {
        lua_pushobject(L, lobj);
    }
}

Conversion py_convert(lua_State *L, PyObject *o) {
    Conversion ret;
    if (o == Py_None || o == Py_False) {
        lua_pushnil(L);
        ret = CONVERTED;
    } else if (o == Py_True) {
        lua_pushnumber(L, 1);
        ret = CONVERTED;
#if PY_MAJOR_VERSION >= 3
        } else if (PyUnicode_Check(o)) {
        Py_ssize_t len;
        char *s = PyUnicode_AsUTF8AndSize(o, &len);
        ret = CONVERTED;
#else
    } else if (PyString_Check(o)) {
        lua_pushstring(L, get_pyobject_as_string(L, o));
        ret = CONVERTED;
    } else if (PyUnicode_Check(o)) {
        char *s = get_pyobject_as_utf8string(L, o);
#endif
        lua_pushstring(L, s);
        ret = CONVERTED;
#if PY_MAJOR_VERSION < 3
    } else if (PyInt_Check(o)) {
        lua_pushnumber(L, PyInt_AsLong(o));
        ret = CONVERTED;
#endif
    } else if (PyLong_Check(o)) {
        lua_pushnumber(L, PyLong_AsLong(o));
        ret = CONVERTED;
    } else if (PyFloat_Check(o)) {
        lua_pushnumber(L, PyFloat_AsDouble(o));
        ret = CONVERTED;
    } else if (LuaObject_Check(o)) {
        lua_pushobject(L, lua_getref(L, ((LuaObject*)o)->ref));
        ret = CONVERTED;
    } else {
        int asindx = 0;
        if (PyObject_IsInstance(o, (PyObject*) &PyList_Type) ||
            PyObject_IsInstance(o, (PyObject*) &PyTuple_Type) ||
            PyObject_IsInstance(o, (PyObject*) &PyDict_Type))
            asindx = 1;
        ret = py_object_wrap_lua(L, o, asindx);
    }
    return ret;
}
