import os
import sys

sys.path.append(os.getcwd())

import lua

print "lib version: ", lua.get_version()
print 'in lua: ', ' | '.join(dir(lua))

PATH = os.path.dirname(os.path.abspath(__file__))

def reversed_relation_call(arg):
    assert arg.sys.path
    assert arg.os.getcwd()

def reversed_relation_iter(arg):
    assert sys in arg
    assert os in arg

class LuaInterpreter(lua.Interpreter):
    def __enter__(self):
        return self
    def __exit__(self, *args, **kwargs):
        pass

def fn(interpreter, index):
    assert interpreter.eval("10") == 10, "error in the int conversion"
    assert interpreter.eval("1.0000001") == 1.0000001, "error in the float conversion"
    assert interpreter.eval("\"a\"") == "a", "error evaluating a"

    interpreter.execute("""
    local os = python.import("os")
    local sys = python.import("sys")
    python.eval("reversed_relation_call")({sys=sys, os=os})
    """)

    interpreter.execute("""
    local os = python.import("os")
    local sys = python.import("sys")
    python.eval("reversed_relation_iter")({sys, os})
    """)

    interpreter.execute("""
    function lua_speak(str, num)
        return format('Hello from %s - %i', str, num)
    end
    """)
    data = interpreter.eval("{a=10, b={1,2,3}, c={'a', 'b', 'c'}, d={a=1,b=2}}")

    assert data['a'] == 10, "table index value error"
    data['a'] = 15

    assert data['a'] == 15, "table index value set error"

    for i in data['c']:
        print 'value c: ', i

    for i in data['d']:
        print i, ": ", data['d'][i]

    lua_speak = interpreter.eval("lua_speak")
    print "callable (%s) function %s" % (callable(lua_speak), lua_speak)

    print(lua_speak(*("Lua", index), **{}))

    data = interpreter.eval("{a={b={c={d={e={f={g={h={i={'a','b','c'}, hi=lua_speak},gh='hello'},"
                       "fg=1.0},ef='a'},de=1},cd={1,2,3}},bc={1,2,3}},ab={1,2,3},}}")

    print data['a']['b']['c']['d']['e']['f']['g']['h']['hi']('lua struct!', index)

    # load test of python!
    interpreter.require(os.path.join(PATH, "..", "lua", "test.lua"))


index = 0
while True:
    with LuaInterpreter(os.environ['BASE_DIR']) as interpreter:
        fn(interpreter, index)  # loop check to lua stack
        index += 1
