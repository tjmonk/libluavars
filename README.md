# libluavars
Lua VarServer Interface

## Overview

The Lua VarServer Interface provides a mechanism to use VarServer variables
inside Lua applications.

The libluavars.so shared object implements these Lua extensions.

The LibLuaVars library provides the following functions

| Function | Description |
| --- | --- |
| get | get a VarServer variable value given its name |
| find | get a VarServer variable handle given its name |
| set | set a VarServer variable value given its name or handle |
| notify | register for VarServer variable notifications |
| wait | wait for a VarServer variable signal |
| validate_start | start a variable validation |
| validate_end | complete a variable validation |
| open_print_session | start a variable print session |
| close_print_session | complete a variable print session |

The libluavars functions can be accessed from inside a LUA application
by loading the library via a require directive.  The Lua interpretter
must know where to look for LUA extension libraries.  The can be
achieved by setting the Lua C Path as follows:

```
export LUA_CPATH=/usr/local/lib/?.so
```

The library can then be referenced as follows:

```
local vars = require("libluavars")
```

The library functions can then be accessed using the vars reference.
For example:

```
a = vars.get("/sys/test/a")
```

## Prerequisites

The libluavars library requires the following components:

- varserver : variable server ( https://github.com/tjmonk/varserver )

It also requires the Lua interpretter, but this is downloaded, built and
installed as part of the libluavars installation script.

## Build

```
./build.sh
```

## Getting variables

VarServer variables can be retrieved by name using vars.get().  The get
function takes a single argument: the name of the VarServer variable to
get.

```
a = vars.get("/sys/test/a")
b = vars.get("/sys/test/b")
f = vars.get("/sys/test/f")
```

## Getting variable handles

You can get a handle to a variable for faster access.  Some functions
(such as notify) can only use a variable handle.

```
hA = vars.find("/sys/test/a");
```

## Setting variable values.

You can set the value of a variable either using its handle or its name.
Given that hA is the handle to the /sys/test/a variable, then the following two
function calls have identical outcomes:

```
vars.set("/sys/test/a", 10);
vars.set(hA, 10);
```

## Setting up variable notifications

Variable notifications are signals received from the VarServer with respect to
a specific variable and an action associated with it.

The following notifications are supported:

| Notification | Description |
| --- | --- |
| NOTIFY_MODIFIED | The client will be notified whenever the value of the specified variable changes |
| NOTIFY_VALIDATE | The client will be asked to validate the variable whenever another client tries to change its value |
| NOTIFY_CALC | The client will be asked to calculate the variable's value whenever another client requests the variable's value |
| NOTIFY_PRINT | The client will be asked to render the variable output whenever another client requests to print the variable |

To set up a notification, you need to specify the variable handle and the type
of notification.

Consider the following use cases:

### We wish to be notified any time the value of a variable changes

```
ha = vars.find( "/sys/test/a" )
vars.notify( ha, NOTIFY_MODIFIED )
```

### We wish to validate the change to a variable before it is applied

```
hB = vars.find( "/sys/test/b" )
vars.notify( hB, NOTIFY_VALIDATE )
```

### We wish to calculate the value of a variable when it is requested

```
hf = vars.find( "/sys/test/f" )
vars.notify( hF, NOTIFY_CALC )
```

### We wish to render the output for a variable when it is requested

```
hC = vars.find( "/sys/test/c" )
vars.notify( hC, NOTIFY_PRINT )
```

## Waiting for notifications

Since VarServer is event driven, we typically wait for notifications we
have requested.  We do this via the vars.wait() function.
The vars.wait() function will block until an event occurs, and then it
will return the signal, and an id.  The usage of the id depends on the
specific signal received.

### Change notification

In the case of a change notification (NOTIFY_MODIFIED), the returned signal
will be SIG_VAR_MODIFIED, and the id will be the handle of the variable
which has been modified.

For example, refer to the following code snippet:

```
    sig,id = vars.wait()
    if sig == SIG_VAR_MODIFIED then
        if id == 1 then
            print(string.format("/SYS/TEST/A changed to %d", vars.get("/sys/test/a") ) )
        end
    end

```

### Validation Notification

A validation notification is received when another client has changed a
variable which we have requested a NOTIFY_VALIDATE signal on.

In this case, the returned signal will be SIG_VAR_VALIDATE, and the
id will be a reference to the validation context.  The validation
is started with the vars.validate_start() function, and completed
with the vars.validate_end() function.

For example:

```
    sig,id = vars.wait()
    if sig == SIG_VAR_VALIDATE then
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
```

### Calc Notifications

A calc notification is received when another client requests the value of a
variable that we have requested a NOTIFY_CALC signal on.

In this case, the signal returned will be SIG_VAR_CALC, and the id will be
a handle to the VarServer variable to be calculated.  The interaction is
completed when a calculated value is written to the variable via the
vars.set() function.

For example:

```
    sig,id = vars.wait()
    if sig == SIG_VAR_CALC then
        print "calculating"
        if id == hF then
            f = f + 3.1415926535
            result = vars.set( id, f )
        end
    end
```

### Print Notifications

A print notification is received when another client requests the VarServer
to print the value of a variable that we have requested a NOTIFY_PRINT signal
on.

In this case the signal received will be SIG_VAR_PRINT, and the id will be
a refernce to a print session.  The vars.open_print_session() function takes
the print session id and starts the print session, returning an output file
descriptor, and a handle to the VarServer variable which needs to be printed.
We can then generate and output the content before closing the print session
with vars.close_print_session().

For example:

```
    sig,id = vars.wait()
    if sig == SIG_VAR_PRINT then
        print "printing"
        ps, hVar = vars.open_print_session( id )
        if hVar == hC then
            ps:write( "Hello from Lua!\n")
            ps:write( string.format( "The counter is %d\n", count ) )
        end
        vars.close_print_session( ps )
        count = count + 1
    end

```

## Example

The complete example below illustrates all of the VarServer notification
handling, and VarServer API calls in a single application.  It is notified when
/sys/test/a is changed.  It validates changes to /sys/test/b and only allows
values less than 10.  It calculates a new value for /sys/test/f every time it
is requested, and it generates a rendering for /sys/test/c when it is requested.

```
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

```

## Run the example

```
varserver &

mkvar -t uint16 -n /sys/test/a
mkvar -t uint32 -n /sys/test/b
mkvar -t float -n /sys/test/f
mkvar -t str -n /sys/test/c

lua test/test.lua &

setvar /sys/test/a 10

setvar /sys/test/b 15

getvar /sys/test/c

getvar /sys/test/f

```