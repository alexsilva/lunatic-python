//
// Created by alex on 12/01/2016.
//

#ifndef LUNATIC_CONSTANTS_H
#define LUNATIC_CONSTANTS_H
// Global Lua sets exposes most api functions
#define PY_API_NAME "python"

// Object wrap names
#define PY_OBJECT "_python_object"
#define PY_OBJECT_BASE "_python_object_base"
#define PY_UNICODE_ENCODING_ERRORHANDLER "_python_unicode_encoding_errorhandler"
#define PY_UNICODE_ENCODING "_python_unicode_encoding"
#define PY_OBJECT_BY_REFERENCE "_python_object_by_reference"
#define PY_API_IS_EMBEDDED "_python_api_is_embedded"

// globals Lua
#define PY_ARGS_ARRAY_FUNC "pyargs_array"
#define PY_KWARGS_FUNC "pykwargs"
#define PY_ARGS_FUNC "pyargs"

// auxiliary (python api)
#define PY_FALSE "False"
#define PY_TRUE "True"
#define PY_NONE "None"
#endif //LUNATIC_CONSTANTS_H
