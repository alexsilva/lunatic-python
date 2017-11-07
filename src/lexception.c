
#include "lexception.h"

void lua_error_fallback(lua_State *L, PyObject *exc, const char *msg) {
    PyErr_Format(exc, "%s\n%s", msg ? msg : "unknown error.", L->traceback->msg);
}