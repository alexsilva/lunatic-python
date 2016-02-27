--
-- Created by IntelliJ IDEA.
-- User: alex
-- Date: 21/09/2015
-- Time: 16:22
-- To change this template use File | Settings | File Templates.
--
local handle, msg

if (not python) then  -- LUA -> PYTHON - LUA
    handle, msg = loadlib(getenv("LIBRARY_PATH"))
    if (not handle or handle == -1) then error(msg) end
    callfromlib(handle, 'luaopen_python')

    assert(python, "undefined python object")

    -- python home
    python.system_init("C:\\Python27");
end
print(format("python ext <%s> is embedded in lua <%s>", python.get_version(), tostring(python.is_embedded())))

local builtins = python.builtins()
local os = python.import("os")

assert(tag(builtins) == python.tag() and tag(os) == python.tag(), "invalid base tag!")

local str = "maçã"
assert(builtins.unicode(str, 'utf-8') == str)

local types = python.import("types")

assert(builtins.isinstance(python.None, types.NoneType), "None type check error")
assert(builtins.isinstance(python.False, types.BooleanType), "False type check error")
assert(builtins.isinstance(python.True, types.BooleanType), "True type check error")

assert(builtins.bool(python.None) == nil, "None object check error")
assert(builtins.bool(python.False) == nil, "False boolean check error")
assert(builtins.bool(python.True) == 1, "True boolean check error")

local sys = python.import("sys")

print("Python sys.path")
builtins.map(function(path) print(path) end, sys.path)

local globals = python.globals()
assert(type(globals) == "userdata", "globals is not a table")

local builtins = python.builtins()
assert(type(builtins) == "userdata", "builtins is not a table")

-- encoding
local encodingdefault = python.get_unicode_encoding()
local errorhandlerdefault = python.get_unicode_encoding_errorhandler()
local errorhandler = "strict"
local encoding = "latin1"

python.set_unicode_encoding(encoding, errorhandler)
assert(python.get_unicode_encoding() == encoding, "invalid encoding check!")
assert(python.get_unicode_encoding_errorhandler() == errorhandler, "invalid encoding errorhandler check!")

-- references
local strref = python.byrefc(builtins.str, "python")
assert(builtins.isinstance(strref, builtins.tuple({builtins.str, builtins.unicode})), "type ref check error!")
assert(strref.upper() == "PYTHON", "invalid ref call") -- Only works in the model references

local key = "name"
local d = builtins.dict(); d[key] = strref.upper()
local object = builtins.type("ObjectC", builtins.tuple(), d)
assert(python.byref(object, key).lower(), "python") -- Only works in the model references

-- set default encondig...
python.set_unicode_encoding(encodingdefault, errorhandlerdefault)

python.execute('import sys')
local sys = python.eval("sys")

sys.MYVAR = 100
local path = "/user/local"
sys.path[0] = path

assert(python.eval("sys.MYVAR") == 100, "set attribute error")
assert(python.eval("sys.path[0]") == path, "set index error")

local string = [[
Lua is an extension programming language designed to support general procedural programming with data description facilities.
Lua also offers good support for object-oriented programming, functional programming, and data-driven programming.
Lua is intended to be used as a powerful, lightweight, embeddable scripting language for any program that needs one.
Lua is implemented as a library, written in clean C, the common subset of Standard C and C++.
]]
assert(builtins.len(string) == strlen(string), "strlen no match")

local struct = python.import("struct")
local bstr = struct.pack('hhl', 1, 2, 3)

local temfile = python.import("tempfile")
local filepath = os.path.join(temfile.gettempdir(), "tempfiletext.bin")
local file = builtins.open(filepath, "wb")
file.write(bstr)
file.close()

local res = struct.unpack('hhl', builtins.open(filepath, "rb").read())
assert(res[0] == 1, "binary string no match")
assert(res[1] == 2, "binary string no match")
assert(res[2] == 3, "binary string no match")

python.execute('import json')
local loadjson = python.eval([[json.loads('{"a": 100, "b": 2000, "c": 300, "items": 100}')]])
assert(type(loadjson) == "userdata", "json type error")

assert(loadjson["a"] == 100)
assert(loadjson["b"] == 2000)
assert(loadjson["c"] == 300)
assert(loadjson["items"] == 100, "key 'items' not found")

local filename = "data.json"

local path = python.import("os").path
assert(path.exists(filename) == nil or path.exists(filename) == 1, "file does not exists")

if (python.import("os.path").exists(filename)) then
    local filedata = builtins.open(filename).read()
    assert(builtins.len(filedata) > 0, "file size is zero")
end

local dict = python.eval("{'a': 'a value'}")
local keys = python.asattr(dict).keys()

assert(python.str(keys) == "['a']", "dict repr object error")
assert(builtins.len(keys) == 1, "dict size no match")
assert(keys[0] == 'a', "dict key no match")

local list = python.eval("[1,2,3,3]")
local lsize = builtins.len(list)

assert(list[0] == 1, "list by index invalid value")
assert(python.asattr(list).pop(0) == 1, "list pop invalid value")
assert(lsize - 1 == builtins.len(list), "size of unexpected list")

local re = python.import('re')
local pattern = re.compile("Hel(lo) world!")
local match = pattern.match("Hello world!")

assert(builtins.len(match.groups()) > 0, "patten without groups")
assert(match.group(1) == "lo",  "group value no match")

local jsond = '["a", "b", "c"]'
local json = python.import('json')

assert(json.loads(jsond)[0] == 'a', "json parsed list 0 index invalid value")
assert(json.loads(jsond)[1] == 'b', "json parsed list 1 index invalid value")
assert(json.loads(jsond)[2] == 'c', "json parsed list 2 index invalid value")

assert(python.eval("1") == 1, "eval int no match")
assert(python.eval("1.0001") == 1.0001, "eval float no match")

python.execute([[
def fn_args(arg):
    assert len(arg) == 5, "no args!"
]])
python.eval("fn_args")({sys, string, re, list, pattern})

python.execute([[
def fn_kwargs(arg):
    for name in ['sys', 'string', 'list', 're', 'pattern']:
        assert arg[name], "arg: %s missing!" % (name,)
]])
python.eval("fn_kwargs")({sys=sys, string=string, re=re, list=list, pattern=pattern})

-- special case
local dict = builtins.dict()
dict[os] = "os module"
dict[sys] = "sys module"
dict["str"] = "simple str"
assert(dict[sys] and dict[os])

local string = python.import("string")
assert(string.split("1", ",")[0] == "1")

-- Ends the Python interpreter, freeing resources to OS
if (python.is_embedded()) then
    python.system_exit()
end