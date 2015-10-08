import os
import sys

sys.path.append(os.getcwd())

import lualib
print dir(lualib)

PATH = os.path.dirname(os.path.abspath(__file__))

def fn(index):
    print(lualib.eval("10"))
    print(lualib.eval("\"a\""))

    lualib.execute("""
    function lua_speak(str, num)
        return format('Hello from %s - %i', str, num)
    end
    """)
    data = lualib.eval("{a=10, b={1,2,3}, c={'a', 'b', 'c'}, d={a=1,b=2}}")

    print data['a'] == 10, data['a']
    data['a'] = 15

    print data['a'] == 15, data['a']

    for i in data['c']:
        print 'value c: ', i

    for i in data['d']:
        print i, ": ", data['d'][i]

    lua_speak = lualib.eval("lua_speak")
    print "callable (%s) function %s" % (callable(lua_speak), lua_speak)

    print(lua_speak(*("Lua", index), **{}))

    data = lualib.eval("{a={b={c={d={e={f={g={h={i={'a','b','c'}, hi=lua_speak},gh='hello'},"
                       "fg=1.0},ef='a'},de=1},cd={1,2,3}},bc={1,2,3}},ab={1,2,3},}}")

    print data['a']['b']['c']['d']['e']['f']['g']['h']['hi']('lua struct!', index)

    # load test of python!

    python = lualib.eval("python")
    python.LUA_EMBED_MODE = True

    lualib.require(os.path.join(PATH, "test.lua"))
    print(python.eval("{'a': 10}"))


for index in range(100000):
    fn(index)  # loop check to lua stack


