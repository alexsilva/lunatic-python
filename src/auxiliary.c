//
// Created by alex on 28/11/2016.
//

/*
** We cannot lookahead without need, because this can lock stdin.
** This flag signals when we need to read a next char.
*/

#include <Python.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "luaconv.h"
#include "pyconv.h"
#include "utils.h"

#define ESC    '%'
#define NEED_OTHER (EOF-1)  /* just some flag different from EOF */


/* Lua source api */
char *luaI_classend(lua_State *L, char *p) {
    switch (*p++) {
        case ESC:
            if (*p == '\0')
                luaL_verror(L, "incorrect pattern (lua_State *L, ends with `%c')", ESC);
            return p + 1;
        case '[':
            if (*p == '^') p++;
            if (*p == ']') p++;
            p = strchr(p, ']');
            if (!p) lua_error(L, "incorrect pattern (lua_State *L, missing `]')");
            return p + 1;
        default:
            return p;
    }
}

/* Reads a single byte from file */
static int _read_single_char(PyObject *o) {
    PyObject *callresult = PyObject_CallMethod(o, "read", "i", 1);
    char *str = PyString_AsString(callresult);
    int result = strcmp(str, "") == 0 ? EOF : *str;
    Py_DECREF(callresult);
    return result;
}

/* Reads a single byte from the file, considering the last one already read */
static int read_single_char(PyObject *o, int *lastreadchar) {
    if (*lastreadchar != 0) {
        int v = *lastreadchar;
        *lastreadchar = 0;
        return v;
    } else {
        return _read_single_char(o);
    }
}

/* Read the file by interpreting the pattern */
static int read_pattern(lua_State *L, PyObject *o, char *p, int *lastreadchar) {
    int inskip = 0;  /* {skip} level */
    int c = NEED_OTHER;
    while (*p != '\0') {
        switch (*p) {
            case '{':
                inskip++;
                p++;
                continue;
            case '}':
                if (!inskip) lua_error(L, "unbalanced braces in read pattern");
                inskip--;
                p++;
                continue;
            default: {
                char *ep = luaI_classend(L, p);  /* get what is next */
                int m;  /* match result */
                if (c == NEED_OTHER) {
                    c = read_single_char(o, lastreadchar);
                }
                m = (c == EOF) ? 0 : luaI_singlematch(L, c, p, ep);
                if (m) {
                    if (!inskip) luaL_addchar(L, c);
                    c = NEED_OTHER;
                }
                switch (*ep) {
                    case '+':  /* repetition (lua_State *L, 1 or more) */
                        if (!m) goto break_while;  /* pattern fails? */
                        /* else go through */
                    case '*':  /* repetition (lua_State *L, 0 or more) */
                        while (m) {  /* reads the same item until it fails */
                            c = read_single_char(o, lastreadchar);
                            m = (c == EOF) ? 0 : luaI_singlematch(L, c, p, ep);
                            if (m && !inskip) luaL_addchar(L, c);
                        }
                        /* go through to continue reading the pattern */
                    case '?':  /* optional */
                        p = ep + 1;  /* continues reading the pattern */
                        continue;
                    default:
                        if (!m) goto break_while;  /* pattern fails? */
                        p = ep;  /* else continues reading the pattern */
                }
            }
        }
    }
    break_while:
    if (c != NEED_OTHER) {
        *lastreadchar = c;
    }
    return (*p == '\0');
}

/* Reads all remaining content from the file */
static int read_file(lua_State *L, PyObject *o, int *lastreadchar) {
    PyObject *callresult = PyObject_CallMethod(o, "read", NULL);
    if (callresult) {
        PyObject *result = callresult;
        if (*lastreadchar != 0) {
            // restore the last character
            char v[] = {(char) *lastreadchar, '\0'};
            result = PyString_FromString(v);
            PyString_Concat(&result, callresult);
            if (!result) lua_raise_error(L, "concatenating part of string of \"%s\"", o);
            Py_DECREF(callresult);
            *lastreadchar = 0;
        }
        if (py_convert(L, result) == CONVERTED)
            Py_DECREF(result);
    } else {
        lua_raise_error(L, "call function python read of \"%s\"", o);
    }
    return 1;
}

/* Function that allows you to read a python file using the same Lua pattern syntax */
void py_readfile(lua_State *L) {
    lua_Object luaObject = lua_getparam(L, 1);
    PyObject *pyFile;
    if (!is_object_container(L, luaObject)) {
        lua_error(L, "is not a file object!");
        return; // warning
    } else {
        pyFile = get_pobject(L, luaObject);
        if (!PyFile_Check(pyFile)) {
            lua_error(L, "is not a file object!");
        }
    }
    static char *options[] = {"*n", "*l", "*a", ".*", "*w", NULL};
    int arg = 2;
    char *p = luaL_opt_string(L, arg++, "*l");
    int lastreadchar = 0;
    do { /* repeat for each part */
        int l = 0, success = 0;
        luaL_resetbuffer(L);
        switch (luaL_findstring(L, p, options)) {
            case 0:  /* number */
                lua_error(L, "read number not implemented!");
                continue;  /* number is already pushed; avoid the "pushstring" */
            case 1:  /* line */
                success = read_pattern(L, pyFile, "[^\n]*{\n}", &lastreadchar);
                break;
            case 2: case 3:  /* file */
                success = read_file(L, pyFile, &lastreadchar); /* always success */
                break;
            case 4:  /* word */
                success = read_pattern(L, pyFile, "{%s*}%S+", &lastreadchar);
                break;
            default:
                success = read_pattern(L, pyFile, p, &lastreadchar);
        }
        l = luaL_getsize(L);
        if (!success && l == 0) return;  /* read fails */
        lua_pushlstring(L, luaL_buffer(L), l);
    } while ((p = luaL_opt_string(L, arg++, NULL)) != NULL);
}
