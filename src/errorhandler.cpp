//
// Created by alex on 04/11/2017.
//
#include <Python.h>
#include <lua.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "utils.h"

using namespace std;

static void string_append(const char *format, string *stack, int nargs, ...) {
    va_list args, fmtargs;
    va_start(args, nargs);
    va_copy(fmtargs, args);
    char *buffstr;
    int buffsize = strlen(format);
    for (int index = 0; index < nargs; index++) {
        buffstr = va_arg(args, char *);
        buffsize += strlen(buffstr);
    }
    va_end(args);
    char buffer[buffsize + 1];
    vsprintf(buffer, format, fmtargs);
    stack->append(buffer);
    va_end(fmtargs);
}

/*
  This function outputs the current lua stack.
  lua_State *L The Lua state to be used.
*/
static int traceback_buffer(lua_State *L, string *stack) {
    char *name;
    int level = 0;
    lua_Object func;
    int currentline;
    char *chunkname;
    int linedefined;
    while ((func = lua_stackedfunction(L, level++)) != LUA_NOOBJECT) {
        lua_funcinfo(L, func, &chunkname, &linedefined);
        currentline = lua_currentline(L, func);
        switch (*lua_getobjname(L, func, &name)) {
            case 'g':
                string_append("function \"%s\" at ", stack, 1, name);
                break;
            case 't':
                string_append("[%s] tag method at ", stack, 1, name);
                break;
            default: {
                if (linedefined == 0) {
                    stack->append("main of ");
                } else if (linedefined < 0) {
                    stack->append("at ");
                } else {
                    lua_pushobject(L, func);
                    lua_call(L, ptrchar "tostring");
                    name = lua_getstring(L, lua_getresult(L, 1));
                    string_append("%s at ", stack, 1, name);
                }
            }
        }
        if (*chunkname == '@') {
            stack->append(chunkname + 1);
        } else if (*chunkname == '(') {
            string_append("C code", stack, 0);
        } else {
            string_append(" string \"%.60s\"", stack, 1, chunkname);
        }
        if (currentline > 0) {
            lua_pushnumber(L, currentline);
            lua_call(L, ptrchar "tostring");
            name = lua_getstring(L, lua_getresult(L, 1));
            string_append(", %s", stack, 1, name);
        }
        stack->append("\n");
    }
    return 0;
}



/*
  This function is registered as the lua active error tag method.
  lua_State *L The Lua state to be used.
*/
void lua_errorfallback(lua_State *L) {
    char *msg = lua_getstring(L, lua_getparam(L, 1));
    string stack("Lua error on configuration (or extension) ");
    stack.append(msg ? msg : "unknown error.");
    stack.append("\n");
    traceback_buffer(L, &stack);
    lua_traceback_append(L, const_cast<char *>(stack.c_str()));
}
