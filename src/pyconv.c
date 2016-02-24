//
// Created by alex on 26/09/2015.
//

#include "pyconv.h"
#include "utils.h"
#include "constants.h"

/* returns the encoding currently being used */
char *get_unicode_encoding(lua_State *L) {
    lua_pushobject(L, lua_getglobal(L, PY_API_NAME));
    lua_pushstring(L, PY_UNICODE_ENCODING);
    return lua_getstring(L, lua_rawgettable(L));
}

/* returns the error handler in the conversion of unicode strings */
char *get_unicode_errorhandler(lua_State *L) {
    lua_pushobject(L, lua_getglobal(L, PY_API_NAME));
    lua_pushstring(L, PY_UNICODE_ENCODING_ERRORHANDLER);
    return lua_getstring(L, lua_rawgettable(L));
}

void set_unicode_string(lua_State *L, char *name, char *value) {
    lua_pushobject(L, lua_getglobal(L, PY_API_NAME));
    lua_pushstring(L, name);
    lua_pushstring(L, value);
    lua_rawsettable(L);
}

/* It indicates whether the object reference should be returned */
int get_isby_reference(lua_State *L) {
    lua_pushobject(L, lua_getglobal(L, PY_API_NAME));
    lua_pushstring(L, PY_OBJECT_BY_REFERENCE);
    return (int) lua_getnumber(L, lua_rawgettable(L));
}

void set_object_by_reference(lua_State *L, int n) {
    lua_pushobject(L, lua_getglobal(L, PY_API_NAME));
    lua_pushstring(L, PY_OBJECT_BY_REFERENCE);
    lua_pushnumber(L, n);
    lua_rawsettable(L);
}

/* python string bytes */
void pyobject_as_string(lua_State *L, PyObject *o, String *str) {
    PyString_AsStringAndSize(o, &str->buff, &str->size);
    if (!str->buff) {
        lua_new_error(L, "converting python string");
    }
}

/* python string unicode */
void pyobject_as_encoded_string(lua_State *L, PyObject *o, String *str) {
    char *encoding = get_unicode_encoding(L);
    char *errorhandler = get_unicode_errorhandler(L);
    PyObject *obj = PyUnicode_AsEncodedString(o, encoding, errorhandler);
    if (!obj) {
        lua_new_error(L, "converting unicode string");
    }
    pyobject_as_string(L, obj, str);
    Py_DECREF(obj);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCDFAInspection"
/**/
py_object *py_object_container(lua_State *L, PyObject *obj, bool asindx) {
    py_object *pobj = malloc(sizeof(py_object));
    if (!pobj) lua_error(L, "failed to allocate memory for container");
    pobj->asindx = asindx;
    pobj->isbase = false;
    pobj->object = obj;
    pobj->isargs = false;
    pobj->iskwargs = false;
    return pobj;
}
#pragma clang diagnostic pop

Conversion push_pyobject_container(lua_State *L, PyObject *obj, bool asindx) {
    lua_pushusertag(L, py_object_container(L, obj, asindx), get_base_tag(L));
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
        py_object *obj = get_py_object(L, lobj);
        lua_pushobject(L, _lua_object_raw(L, obj->object, 0, NULL));
    } else {
        lua_pushobject(L, lobj);
    }
}

static Conversion py_object_wrapper(lua_State *L, PyObject *o) {
    bool asindx = false;
    if (PyObject_IsInstance(o, (PyObject*) &PyList_Type) ||
        PyObject_IsInstance(o, (PyObject*) &PyTuple_Type) ||
        PyObject_IsInstance(o, (PyObject*) &PyDict_Type))
        asindx = true;
    return push_pyobject_container(L, o, asindx);
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
        if (get_isby_reference(L)) {
            ret = py_object_wrapper(L, o);
        } else {
            String str;
            pyobject_as_string(L, o, &str);
            lua_pushlstring(L, str.buff, str.size);
            ret = CONVERTED;
        }
    } else if (PyUnicode_Check(o)) {
        if (get_isby_reference(L)) {
            ret = py_object_wrapper(L, o);
        } else {
            String str;
            pyobject_as_encoded_string(L, o, &str);
            lua_pushlstring(L, str.buff, str.size);
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
        ret = py_object_wrapper(L, o);
    }
    return ret;
}
