//
// Created by alex on 26/09/2015.
//

#include "pyconv.h"
#include "luaconv.h"
#include "utils.h"

PyObject *LuaObject_New(lua_State *L, int n) {
    LuaObject *obj = PyObject_New(LuaObject, &LuaObject_Type);
    if (obj) {
        lua_pushobject(L, lua_getparam(L, n));
        obj->ref = lua_ref(L, 1);
        obj->refiter = 0;
    }
    return (PyObject*) obj;
}

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
    o = PyUnicode_AsUTF8String(o);
    if (!o) {
        lua_new_error(L, "converting unicode string");
    }
    return get_pyobject_as_string(L, o);
}

int py_object_wrap_lua(lua_State *L, PyObject *pobj, int asindx) {
    Py_INCREF(pobj);
    Py_INCREF(pobj);

    lua_Object ltable = lua_createtable(L);

    set_table_userdata(L, ltable, POBJECT, pobj);
    set_table_number(L, ltable, ASINDX, asindx);
    set_table_userdata(L, ltable, "base", Py_False);  // derived

    // register all tag methods
    int tag = get_base_tag(L);
    lua_pushobject(L, ltable);
    lua_settag(L, tag);

    // returning table
    lua_pushobject(L, ltable);
    return 1;
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
        py_object *obj = get_py_object(L, 1);
        lua_pushobject(L, _lua_object_raw(L, obj->o, 0, NULL));
        free(obj);
    } else {
        lua_pushobject(L, lobj);
    }
}

int py_convert(lua_State *L, PyObject *o) {
    int ret = 0;
    if (o == Py_None || o == Py_False) {
        lua_pushnil(L);
        ret = 1;
    } else if (o == Py_True) {
        lua_pushnumber(L, 1);
        ret = 1;
#if PY_MAJOR_VERSION >= 3
        } else if (PyUnicode_Check(o)) {
        Py_ssize_t len;
        char *s = PyUnicode_AsUTF8AndSize(o, &len);
#else
    } else if (PyString_Check(o)) {
        lua_pushstring(L, get_pyobject_as_string(L, o));
        ret = 1;
    } else if (PyUnicode_Check(o)) {
        char *s = get_pyobject_as_utf8string(L, o);
#endif
        lua_pushstring(L, s);
        ret = 1;
#if PY_MAJOR_VERSION < 3
    } else if (PyInt_Check(o)) {
        lua_pushnumber(L, PyInt_AsLong(o));
        ret = 1;
#endif
    } else if (PyLong_Check(o)) {
        lua_pushnumber(L, PyLong_AsLong(o));
        ret = 1;
    } else if (PyFloat_Check(o)) {
        lua_pushnumber(L, PyFloat_AsDouble(o));
        ret = 1;
    } else if (LuaObject_Check(o)) {
        lua_pushobject(L, lua_getref(L, ((LuaObject*)o)->ref));
        ret = 1;
    } else {
        int asindx = 0;
        if (PyList_Check(o) || PyTuple_Check(o) || PyDict_Check(o))
            asindx = 1;
        ret = py_object_wrap_lua(L, o, asindx);
    }
    return ret;
}
