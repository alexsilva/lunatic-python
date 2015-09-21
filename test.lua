--
-- Created by IntelliJ IDEA.
-- User: alex
-- Date: 21/09/2015
-- Time: 16:22
-- To change this template use File | Settings | File Templates.
--
print(getenv("LIBRARY_PATH"))
handle, msg = loadlib(getenv("LIBRARY_PATH"))

if (not handle or handle == -1) then error(msg) end

callfromlib(handle, 'luaopen_python')

print(python)

local builtins = python.builtins()

print(builtins.len)

