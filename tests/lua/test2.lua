--
-- Created by IntelliJ IDEA.
-- User: alex
-- Date: 24/09/2015
-- Time: 18:53
-- To change this template use File | Settings | File Templates.
--
if (not python) then
    local handle, msg = loadlib(getenv("LIBRARY_PATH"))
    if (not handle or handle == -1) then error(msg) end

    callfromlib(handle, 'luaopen_python')
    assert(python, "undefined python object")

    -- python home
    python.system_init("C:\\Python27");
end

local builtins = python.builtins()
local globals = python.globals()

python.execute([[
def TestFunc(*args, **kwargs):
    _args = (1, 2, 3, 4, 5, ('a', 'c', 'd', 'e', {'a': 1, 'c': 3, 'b': 2}))
    _kwargs = {
        'host': '127.0.0.1',
        'user': 'user',
        'passwd': '12345&*',
        'db': 'mysqdb'
    }
    assert args == _args, "[TestFunc] args no match!"

    for k in _kwargs:
        assert k in kwargs, "[TestFunc] key not found"
        assert kwargs[k] == _kwargs[k], "[TestFunc] data no match!"
    return True
]])

python.execute([[
def TestFunc2(limit, *args, **kwargs):
    assert range(1,limit) == list(args[0]), '[TestFunc2] args no match!'
]])

python.execute([[
def TestFunc3(*args, **kwargs):
    _args = (1,2,3,4)
    _kwargs = {'a': 1, 'b': 2, 'c': 3}
    assert _args == args, '[TestFunc3] args no match!'
    for k in _kwargs:
        assert k in kwargs, "[TestFunc3] key not found"
        assert kwargs[k] == _kwargs[k], "[TestFunc3] data no match!"
]])

python.execute([[
def TestFunc4(*args, **kwargs):
    _kwargs = {'a': 1, 'b': 2, 'c': 3}
    assert args == (), '[TestFunc4] args no match!'
    for k in _kwargs:
        assert k in kwargs, "[TestFunc4] key not found"
        assert kwargs[k] == _kwargs[k], "[TestFunc4] data no match!"
]])

python.execute([[
def TestFunc5(*args, **kwargs):
    _args = (1,2,3,4)
    assert args == _args, '[TestFunc5] args no match!'
    assert kwargs == {}, '[TestFunc5] kwargs no match!'
]])

python.execute([[
def TestFunc6(*args, **kwargs):
    assert args == (), '[TestFunc6] args no match!'
    assert kwargs == {}, '[TestFunc6] kwargs no match!'
]])

python.execute([[
def TestFunc7(*args):
    assert cmp(args, ('a', 'b', 'c', 1, 2, 3)) == 0, '[TestFunc7] args no match!'
    return args
]])

local index = 0
while (index < 10) do
    local args = python.args(1,2,3,4,5,{'a','c','d','e', {a=1,b=2,c=3}})
    local kwargs = python.kwargs({host='127.0.0.1', user='user', passwd='12345&*', db='mysqdb'})
    globals.TestFunc(args, kwargs)
    index = index + 1
end

globals.TestFunc2(5, builtins.range(1,5))
globals.TestFunc2(5, python.args(1,2,3,4))

-- global args, kwargs
globals.TestFunc3(pyargs(1,2,3,4), pykwargs{a=1, b=2, c=3})
globals.TestFunc4(pykwargs{a=1, b=2, c=3})
globals.TestFunc5(pyargs(1,2,3,4))
globals.TestFunc6(pyargs(), pykwargs{})

-- test table -> tuple
globals.TestFunc7(pyargs_array(globals.TestFunc7(pyargs_array{'a', 'b', 'c', 1, 2, 3})))

-- lua callback teste
function shorfn(a, b)
    return % builtins.cmp(a, b)
end

local res_expected = builtins.iter({-5, -1, 0, 1, 4, 7, 10, 100})
local result = builtins.sorted({1, -1, 4, 10, 0, 100, 7, -5}, shorfn)
local v = builtins.next(res_expected);

while (v) do
    if (not python.eval(tostring(v)..[[ in ]]..builtins.str(result))) then
        error("value "..tostring(v).." not in "..builtins.str(result))
    end
    v = builtins.next(res_expected, python.False);
end

-- Ends the Python interpreter, freeing resources to OS
if (python.is_embedded()) then
    python.system_exit()
end