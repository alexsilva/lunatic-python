//
// Created by alex on 26/09/2015.
//

#include "pyconv.h"
#include "utils.h"
#include "constants.h"

/* python string bytes */
void get_pyobject_string_buffer(lua_State *L, PyObject *obj, String *str) {
    PyString_AsStringAndSize(obj, &str->buff, &str->size);
    if (!str->buff) lua_new_error(L, "converting string");
}

/* python string unicode as string bytes */
PyObject *get_pyobject_encoded_string_buffer(lua_State *L, PyObject *obj, String *str) {
    char *encoding = python_getstring(L, PY_UNICODE_ENCODING);
    char *errorhandler = python_getstring(L, PY_UNICODE_ENCODING_ERRORHANDLER);
    PyObject *pyStr = PyUnicode_AsEncodedString(obj, encoding, errorhandler);
    if (!pyStr) lua_new_error(L, "converting unicode string");
    get_pyobject_string_buffer(L, pyStr, str);
    return pyStr;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
/**/
py_object *py_object_container(lua_State *L, PyObject *obj, bool asindx) {
    py_object *pobj = malloc(sizeof(py_object));
    if (!pobj) lua_error(L, "failed to allocate memory for container");
    pobj->asindx = asindx;
    pobj->object = obj;
    pobj->isargs = false;
    pobj->iskwargs = false;
    return pobj;
}
#pragma clang diagnostic pop

Conversion push_pyobject_container(lua_State *L, PyObject *obj, bool asindx) {
    lua_pushusertag(L, py_object_container(L, obj, asindx), python_api_tag(L));
    return WRAPPED;
}

lua_Object py_object_raw(lua_State *L, PyObject *obj,
                         lua_Object lptable, PyObject *lpkey) {
    lua_Object ltable = lua_createtable(L);
    if (PyObject_IsDictInstance(obj)) {
        PyObject *iterator = PyObject_GetIter(obj); // iterator keys
        if (!iterator) lua_new_error(L, "failed to create the dictionary iterator");
        PyObject *key, *value;
        while ((key = PyIter_Next(iterator))) {
            value = PyObject_GetItem(obj, key);
            if (!value) {
                Py_DECREF(iterator); Py_DECREF(key);
                lua_raise_error(L, "failed to convert the key value %s", key);
            }
            if (PyObject_IsDictInstance(value) || PyObject_IsListInstance(value) ||
                PyObject_IsTupleInstance(value)) {
                if (py_object_raw(L, value, ltable, key) == LUA_NOOBJECT) {
                    Py_DECREF(key); Py_DECREF(value); Py_DECREF(iterator);
                    lua_raise_error(L, "raw type not supported \"%s\"", value);
                } else {
                    Py_DECREF(value);
                }
            } else {
                lua_pushobject(L, ltable);
                if (py_convert(L, key) == CONVERTED) Py_DECREF(key);
                if (py_convert(L, value) == CONVERTED) Py_DECREF(value);
                lua_settable(L);
            }
        }
        Py_DECREF(iterator);
        if (PyErr_Occurred()) {
            lua_raise_error(L, "failure iterating dictionary: %s", obj);
        }
    } else if (PyObject_IsListInstance(obj) || PyObject_IsTupleInstance(obj)) {
        Py_ssize_t index, size = PyObject_Size(obj);
        PyObject *ikey, *value;
        for (index = 0; index < size; index++) {
            ikey = PyInt_FromSsize_t(index);
            value = PyObject_GetItem(obj, ikey);
            Py_DECREF(ikey);
            if (PyObject_IsDictInstance(value) || PyObject_IsListInstance(value) ||
                PyObject_IsTupleInstance(value)) {
                ikey = PyInt_FromSsize_t(index + 1);
                if (py_object_raw(L, value, ltable, ikey) == LUA_NOOBJECT) {
                    Py_DECREF(ikey); Py_DECREF(value);
                    lua_raise_error(L, "raw type not supported \"%s\"", value);
                } else {
                    Py_DECREF(value);
                }
            } else {
                lua_pushobject(L, ltable);
                lua_pushnumber(L, index + 1);
                if (py_convert(L, value) == CONVERTED) Py_DECREF(value);
                lua_settable(L);
            }
        }
    } else {
        return LUA_NOOBJECT; // error: invalid type
    }
    if (lptable && lpkey) {
        lua_pushobject(L, lptable);
        if (py_convert(L, lpkey) == CONVERTED) Py_DECREF(lpkey);
        lua_pushobject(L, ltable);
        lua_settable(L);
    }
    return ltable;
}

/* Convert types in Python directly to the Lua */
void pyobj2table(lua_State *L) {
    lua_Object lobj = lua_getparam(L, 1);
    if (is_object_container(L, lobj)) {
        py_object *obj = get_py_object(L, lobj);
        lua_Object retval = py_object_raw(L, obj->object, 0, NULL);
        if (retval == LUA_NOOBJECT)
            lua_raise_error(L, "raw type not supported \"%s\"", obj->object);
        lua_pushobject(L, retval);
    } else {
        lua_pushobject(L, lobj);
    }
}

static Conversion xpush_pyobject_container(lua_State *L, PyObject *obj) {
    bool asindx = false;
    if (PyObject_IsListInstance(obj) || PyObject_IsTupleInstance(obj) ||
        PyObject_IsDictInstance(obj))
        asindx = true;
    return push_pyobject_container(L, obj, asindx);
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
        lua_pushstring(L, s);
        ret = CONVERTED;
#else
    } else if (PyString_Check(o)) {
        if (is_byref(L)) {
            ret = xpush_pyobject_container(L, o);
        } else {
            String str;
            get_pyobject_string_buffer(L, o, &str);
            lua_pushlstring(L, str.buff, str.size);
            ret = CONVERTED;
        }
    } else if (PyUnicode_Check(o)) {
        if (is_byref(L)) {
            ret = xpush_pyobject_container(L, o);
        } else {
            String str;
            PyObject *pyStr = get_pyobject_encoded_string_buffer(L, o, &str);
            lua_pushlstring(L, str.buff, str.size);
            Py_DECREF(pyStr);
            ret = CONVERTED;
        }
#endif
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
        ret = xpush_pyobject_container(L, o);
    }
    return ret;
}
