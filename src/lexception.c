
#include "lexception.h"

#ifdef python_COMPAT
#include "utils.h"
#endif

void lua_error_fallback(lua_State *L, PyObject *exc, const char *msg) {
    const char *traceback = lua_traceback_message(L);
    const char *format = traceback == NULL ? "%s" :  "%s\n%s";
    PyErr_Format(exc, format, msg ? msg : "unknown error.", traceback);
    free((void *) traceback);
}