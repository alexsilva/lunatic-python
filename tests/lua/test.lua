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
    python.system_init(getenv("PYTHON27_HOME") or "C:\\Python27");
end
print(format("python ext <%s> is embedded in lua <%s>", python.get_version(), tostring(python.is_embedded())))

local builtins = python.builtins()
local os = python.import("os")

local filepath = os.path.join(os.getcwd(), 'popen.txt')
local popen = builtins.open(filepath, "w+")

popen.write("11111111111 assdfasdfasdf x a b c dddddd")
popen.flush()
popen.seek(0)

a, b, c = python.readfile(popen, "*w", "*w", ".*")

print("value: ["..a.."]")
print("value: ["..b.."]")
print("value: ["..c.."]")

-- clean
popen.close()
os.remove(filepath)

assert(tag(builtins) == python.tag() and tag(os) == python.tag(), "invalid tag!")

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
assert(type(globals) == "userdata", "globals is not a userdata")

local builtins = python.builtins()
assert(type(builtins) == "userdata", "builtins is not a userdata")

assert(builtins.isinstance(python.tuple{1,2,3}, builtins.tuple), "tuple error!")
assert(builtins.isinstance(python.list{1,2,3,4,5}, builtins.list), "list error!")
assert(builtins.isinstance(python.dict{a=10, b=builtins.range(5)}, builtins.dict), "dict error!")

assert(builtins.isinstance(python.asargs(builtins.list(python.dict{a = 1, b = 2, c = 3})), builtins.tuple), "#1 tuple expected")
assert(builtins.isinstance(python.asargs(python.tuple{"a", "b", "c"}), builtins.tuple), "#2 tuple expected")
assert(builtins.isinstance(python.asargs(python.list{1,2,3}), builtins.tuple), "#3 tuple expected")
assert(builtins.isinstance(python.asargs(builtins.range(3)), builtins.tuple), "#4 tuple expected")

assert(builtins.isinstance(python.askwargs(python.dict{a = 1, b = 2, c = 3}), builtins.dict), "#1 dict expected")

local l = python.list{"a", "b", "c", "d"}
assert(python.slice(l, 2, -1)[0] == "c", "slice error!")

local d = builtins.dict({["1.0"] = "a", [2] = "b"})
assert(d["1.0"] == "a", "#1 dict error!")
assert(d[2] == "b", "#2 dict error!")

d = {[1.5] = "a", [2.6] = 5, [3] = 6}
local x = builtins.list(d)
local y = builtins.dict(d)

builtins.map(function(v)
    assert(python.asattr(% x).count(v) > 0, "# list check error!")
end, {1.5, 2.6, 3})

assert(y[1.5] == "a", "#1 dict value error!")
assert(y[2.6] == 5, "#2 dict value error!")
assert(y[3] == 6, "#3 dict value error!")

d = {[1] = "a", [2] = 5, [3] = "b" }
x = builtins.list(d)
builtins.map(function(v)
    assert(python.asattr(% x).count(v) > 0, "## list check error!")
end, {"a", 5, "b"})

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

assert(python.repr(keys) == "['a']", "dict repr object error")
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

local jvl = json.loads('{"a" : 1}')
local jvlc = python.raw(jvl)

assert(builtins.isinstance(jvl, builtins.dict), "dict check: type error")
assert(type(jvlc) == "table", "raw type: invalid type!")
assert(jvlc["a"] == 1, "raw type: key 'a' not found!")

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

local X = builtins.type("X", builtins.tuple(), python.dict{lang = "lua"})

-- Complex use of references
python.byrefc(builtins.map, function(s)
        assert(python.tag() == tag(s), "str|2: invalid ref!")
        assert(% builtins.len(s) == 1, "str|2 len error!")
    end,
    python.byrefc(builtins.map, function(s)
            assert(python.tag() == tag(s), "str|1: invalid ref!")
            assert(% builtins.len(s) == 1, "str|1 len error!")
            local value  = python.byrefc(% builtins.unicode, s, python.get_unicode_encoding())
            local lang = % X.lang
            assert(python.tag() == tag(lang), "lang invalid ref!")
            return python.byrefc(% builtins.unicode.upper, value)
        end,
        builtins.str.split("a,b,c,d,e,d,f,g", ","))
)

-- special case
local dict = builtins.dict()
dict[os] = "os module"
dict[sys] = "sys module"
dict["str"] = "simple str"
assert(dict[sys] and dict[os])

local string = python.import("string")
assert(string.split("1", ",")[0] == "1")

-- Test the conversion of a table with many items
local random = python.import("random")
local tbl = {}
local index = 1
local limit = 50000
while (index < limit + 1) do
    tbl[index] = random.randrange(2000, 3000)
    index = index + 1
end
python.execute([[
def check_kwargs_long(**kwargs):
    assert len(kwargs['table']) == ]] .. tostring(limit) ..[[,'python: list size error!'
]])

python.eval("check_kwargs_long")(pykwargs{table = tbl})

-- Ends the Python interpreter, freeing resources to OS
if (python.is_embedded()) then
    python.system_exit()
end