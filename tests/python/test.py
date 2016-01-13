import os
import sys

sys.path.append(os.getcwd())

import lua
print 'in lua: ', ' | '.join(dir(lua))

PATH = os.path.dirname(os.path.abspath(__file__))

class LuaInterpreter(lua.Interpreter):
    def __enter__(self):
        return self
    def __exit__(self, *args, **kwargs):
        pass

def fn(interpreter, index):
    print(interpreter.eval("10"))
    print(interpreter.eval("\"a\""))

    interpreter.execute("""
    function lua_speak(str, num)
        return format('Hello from %s - %i', str, num)
    end
    """)
    data = interpreter.eval("{a=10, b={1,2,3}, c={'a', 'b', 'c'}, d={a=1,b=2}}")

    print data['a'] == 10, data['a']
    data['a'] = 15

    print data['a'] == 15, data['a']

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
