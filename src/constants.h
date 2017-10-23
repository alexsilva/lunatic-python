//
// Created by alex on 12/01/2016.
//

#ifndef LUNATIC_CONSTANTS_H
#define LUNATIC_CONSTANTS_H
// Global Lua sets exposes most api functions
#define PY_API_NAME (ptrchar "python")

// Object wrap names
#define PY_OBJECT_BY_REFERENCE (ptrchar "_object_by_reference")
#define PY_LUA_TABLE_CONVERT (ptrchar "_lua_table_convert")
#define PY_ERRORHANDLER_STACK  (ptrchar "_errorHandlerStack")

#define LUA_INSIDE_PYTHON (ptrchar "_lua_interpretrer_embedded")

// globals Lua
#define PY_ARGS_ARRAY_FUNC (ptrchar "pyargs_array")
#define PY_KWARGS_FUNC (ptrchar "pykwargs")
#define PY_ARGS_FUNC (ptrchar "pyargs")

// auxiliary (python api)
#define PY_FALSE (ptrchar "False")
#define PY_TRUE (ptrchar "True")
#define PY_NONE (ptrchar "None")
#endif //LUNATIC_CONSTANTS_H
