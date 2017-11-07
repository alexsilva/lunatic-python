
#include "lexception.h"

#ifdef python_COMPAT
#include "utils.h"
#endif

void lua_error_fallback(lua_State *L, PyObject *exc, const char *msg) {
    PyErr_Format(exc, "%s\n%s", msg ? msg : "unknown error.",
                 lua_traceback_message(L));
}