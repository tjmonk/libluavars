--------------------------------------------------------------------------------
--MIT License
--
--Copyright (c) 2023 Trevor Monk
--
--Permission is hereby granted, free of charge, to any person obtaining a copy
--of this software and associated documentation files (the "Software"), to deal
--in the Software without restriction, including without limitation the rights
--to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
--copies of the Software, and to permit persons to whom the Software is
--furnished to do so, subject to the following conditions:
--
--The above copyright notice and this permission notice shall be included in all
--copies or substantial portions of the Software.
--
--THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
--IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
--FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
--AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
--LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
--OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
--SOFTWARE.
--------------------------------------------------------------------------------

-- test lua script for the vars library

local vars = require("libluavars")
local a
local b
local c
local f
local hA;
local sig;
local id;
local hVar;
local value;
local result;
local EOK = 0
local ERANGE = 34
local ps
local count = 0;

a = vars.get("/sys/test/a")
b = vars.get("/sys/test/b")
c = vars.get("/sys/test/c")
f = vars.get("/sys/test/f")

if ( a ~= nil ) then
    print( string.format("a = %d", a) )
    vars.set( "/sys/test/a", a+1)
    a = vars.get("/sys/test/a")
    print( string.format("a = %d", a) )
end

if b ~= nil then
    print( string.format("b = %d", b ) )
end

if c ~= nil then
    print( string.format("c = %s", c ) )
end

if f ~= nil then
    print( string.format("f = %f", f ) )
end

local hA = vars.find("/sys/test/a")
local hB = vars.find("/sys/test/b")
local hF = vars.find("/sys/test/f")
local hC = vars.find("/sys/test/c")

-- setup notifications
vars.notify( hA, NOTIFY_MODIFIED )
vars.notify( hB, NOTIFY_VALIDATE )
vars.notify( hF, NOTIFY_CALC )
vars.notify( hC, NOTIFY_PRINT )

while( 1 )
do
    sig,id = vars.wait()
    print( string.format( "Received signal: %d id: %d", sig, id ) )
    if sig == SIG_VAR_MODIFIED then
        if id == 1 then
            print(string.format("/SYS/TEST/A changed to %d", vars.get("/sys/test/a") ) )
        end
    elseif sig == SIG_VAR_VALIDATE then
        print "validating"
        hVar, value = vars.validate_start(id)
        if hVar == hB then
            if value < 10 then
                -- allow the write
                print(string.format("Allow write of %d to /sys/test/b",value))
                result = EOK
            else
                -- disallow the write
                print(string.format("Disallow write of %d to /sys/test/b",value))
                result = ERANGE
            end

            vars.validate_end( id, result )
        end
    elseif sig == SIG_VAR_CALC then
        print "calculating"
        if id == hF then
            f = f + 3.1415926535
            result = vars.set( id, f )
        end
    elseif sig == SIG_VAR_PRINT then
        print "printing"
        ps, hVar = vars.open_print_session( id )
        if hVar == hC then
            ps:write( "Hello from Lua!\n")
            ps:write( string.format( "The counter is %d\n", count ) )
        end
        vars.close_print_session( ps )
        count = count + 1
    end
end
