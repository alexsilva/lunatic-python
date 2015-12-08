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
end

assert(python, "undefined python object")

-- python home
python.system_init("C:\\Python27");

print(format("python is embedded in lua <%s>", tostring(python.is_embedded())))

print("Python sys.path")
local builtins = python.builtins()

local str = "maçã"
assert(builtins.unicode(str, 'utf-8') == str)

assert(builtins.bool(python.False) == nil, "False boolean check error")
assert(builtins.bool(python.True) == 1, "True boolean check error")

local sys = python.import("sys")
local index = builtins.len(sys.path)
while (index > 0) do
    print(index, sys.path[index - 1])
    index = index - 1
end

local globals = python.globals()
assert(type(globals) == "table", "globals is not a table")

local builtins = python.builtins()
assert(type(builtins) == "table", "builtins is not a table")

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
assert(builtins.len(string) == strlen(string))

python.execute('import json')
local loadjson = python.eval([[json.loads('{"a": 100, "b": 2000, "c": 300, "items": 100}')]])
assert(type(loadjson) == "table")

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

-- Ends the Python interpreter, freeing resources to OS
if (not python.LUA_EMBED_MODE) then
    python.system_exit()
end