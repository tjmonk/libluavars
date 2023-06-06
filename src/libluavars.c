/*==============================================================================
MIT License

Copyright (c) 2023 Trevor Monk

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
==============================================================================*/

/*!
 * @defgroup libluavars libluavars
 * @brief Shared object library to interface lua with the variable server
 * @{
 */

/*============================================================================*/
/*!
@file libluavars.c

    Shared object library to interface lua programs with the variable server

    The libluavars.so library provides a mechanism to create interface
    lua programs with the variable server.  The library provides a
    lua library named "var" which provides functions to get, set, notify,
    etc.

*/
/*============================================================================*/


/*==============================================================================
        Includes
==============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <varserver/varserver.h>
#include <varserver/var.h>
#include <lua.h>
#include <lauxlib.h>

/*==============================================================================
        Private definitions
==============================================================================*/

/*! macro for setting global constant values in Lua */
#define lua_setConst(name) { lua_pushnumber( L, name ); \
                             lua_setglobal(L, #name ); }

/*==============================================================================
        Type Definitions
==============================================================================*/

/*! Print Session Object */
typedef struct _LuaPrintSession
{
    /*! lua stream */
    luaL_Stream stream;

    /*! file descriptor for the underlying file */
    int fd;

    /*! print session identifier */
    uint32_t id;

    /*! handle to the variable to be printed */
    VAR_HANDLE hVar;
} LuaPrintSession;

/*==============================================================================
        Private function declarations
==============================================================================*/
int luaopen_vars( lua_State *L );

static int var_get( lua_State *L );
static int var_set( lua_State *L );
static int global_unload(lua_State *L );
static int var_find( lua_State *L );
static int var_notify( lua_State *L );
static int var_wait( lua_State *L );
static int var_validate_start( lua_State *L );
static int var_validate_end( lua_State *L );
static int var_open_print_session( lua_State *L );
static int var_close_print_session( lua_State *L );
static void setup_globals( lua_State *L );

/*==============================================================================
        Local/Private variables
==============================================================================*/

/*! handle to the variable server */
static VARSERVER_HANDLE hVarServer = NULL;

/*! mapping of luavars library functions to c functions */
static const luaL_Reg vars_lib[] = {
    { "get", var_get },
    { "find", var_find },
    { "set", var_set },
    { "notify", var_notify },
    { "wait", var_wait },
    { "validate_start", var_validate_start },
    { "validate_end", var_validate_end },
    { "open_print_session", var_open_print_session },
    { "close_print_session", var_close_print_session },
    { "__unload", global_unload },
    { NULL, NULL }
};

/*==============================================================================
        Function definitions
==============================================================================*/

void __attribute__ ((constructor)) initLibrary(void) {
 //
 // Function that is called when the library is loaded
 //
}
void __attribute__ ((destructor)) cleanUpLibrary(void) {
 //
 // Function that is called when the library is »closed«.
 //
}

/*============================================================================*/
/*  global_unload                                                             */
/*!
    Unload the lua vars library

    This function close the connection to the variable server when the
    lua vars libary is unloaded.

    @param[in]
        L
            pointer to the lua state

    @return always returns 0

==============================================================================*/
static int global_unload(lua_State *L )
{
    (void)L;

    if( hVarServer != NULL )
    {
        (void)VARSERVER_Close(hVarServer);
    }

    return 0;
}

/*============================================================================*/
/*  luaopen_vars                                                              */
/*!
    Entry point for the lua vars library

    This function is the entry point for the lua vars library which
    is invoked by the lua require('vars') statement

    @param[in]
        L
            pointer to the lua state

    @return always returns 1

==============================================================================*/
int luaopen_libluavars( lua_State *L )
{
    if( L != NULL )
    {
        if( hVarServer == NULL )
        {
            hVarServer = VARSERVER_Open();
        }

        lua_newtable( L );
        luaL_setfuncs( L, vars_lib, 0 );

        /* set up the global variables */
        setup_globals( L );
    }

    return 1;
}

/*============================================================================*/
/*  setup_globals                                                             */
/*!
    Set up global variables on the Lua stack

    This setup_globals function sets up the global variables on the
    lua stack so they can be accessed by lua programs

    @param[in]
        L
            pointer to the lua state

==============================================================================*/
static void setup_globals( lua_State *L )
{
    if( L != NULL )
    {
        lua_setConst( SIG_VAR_MODIFIED );
        lua_setConst( SIG_VAR_CALC );
        lua_setConst( SIG_VAR_VALIDATE );
        lua_setConst( SIG_VAR_PRINT );
        lua_setConst( NOTIFY_MODIFIED );
        lua_setConst( NOTIFY_CALC );
        lua_setConst( NOTIFY_VALIDATE );
        lua_setConst (NOTIFY_PRINT );
    }
}

/*============================================================================*/
/*  var_get                                                                   */
/*!
    var.get()

    This var.get() function interfaces to the VAR_Get() function
    in the libvarserver.so library

    The name of the variable is passed in on the lua stack
    and the variable value is pushed back onto the lua stack

    @param[in]
        L
            pointer to the lua state

    @return always returns 0

==============================================================================*/
static int var_get( lua_State *L )
{
    int result = 0;
    const char *name;
    size_t len;
    VAR_HANDLE hVar;
    VarObject var;
    char buf[BUFSIZ];

    if( L != NULL )
    {
        memset( &var, 0, sizeof( VarObject ) );

        name = luaL_checklstring( L, 1, &len );
        if( name != NULL )
        {
            hVar = VAR_FindByName( hVarServer, (char *)name );
            if( hVar != VAR_INVALID )
            {
                /* set up string buffer */
                var.val.str = buf;
                var.len = BUFSIZ;

                if( VAR_Get( hVarServer, hVar, &var ) == EOK )
                {
                    result = 0;
                    switch( var.type )
                    {
                        case VARTYPE_STR:
                            lua_pushstring( L, var.val.str );
                            result = 1;
                            break;

                        case VARTYPE_UINT16:
                            lua_pushnumber( L, var.val.ui );
                            result = 1;
                            break;

                        case VARTYPE_UINT32:
                            lua_pushnumber( L, var.val.ul );
                            result = 1;
                            break;

                        case VARTYPE_FLOAT:
                            lua_pushnumber( L, var.val.f );
                            result = 1;
                            break;

                        default:
                            break;
                    }
                }
            }
        }
    }

    if( result == 0 )
    {
        lua_pushnil( L );
    }
    return result;
}

/*============================================================================*/
/*  var_set                                                                   */
/*!
    var.set()

    This var.set() function interfaces to the VAR_SetStr() function
    in the libvarserver.so library

    The name of the variable is passed in on the lua stack
    The value to be set is passed in as a string on the lua stack
    and the result is pushed back onto the lua stack.
    If the set fails, then nil is pusedh back onto the lua stack

    @param[in]
        L
            pointer to the lua state

    @return always returns 0

==============================================================================*/
static int var_set( lua_State *L )
{
    char *name;
    char *value;
    int result = 0;
    size_t len;
    VAR_HANDLE hVar = VAR_INVALID;
    VarType type;
    const char *argtype;

    if( L != NULL )
    {
        /* check if a variable name was supplied */

        argtype = luaL_typename( L, 1 );

        if( strcmp( argtype, "string") == 0 )
        {
            name = (char *)luaL_checklstring(L, 1, &len );
            if( name != NULL )
            {
                hVar = VAR_FindByName( hVarServer, name );
            }
        }
        else if( strcmp( argtype, "number" ) == 0 )
        {
            /* get the variable handle from the lua stack */
            hVar = luaL_checknumber( L, 1 );
        }

        /* get the value from the lua stack */
        value = (char *)luaL_checklstring( L, 2, &len );

        if( hVar != VAR_INVALID )
        {
            /* get the variable type so we can convert the
            string to a VarObject */
            if( VAR_GetType( hVarServer, hVar, &type ) == EOK )
            {
                /* set the variable value from the string */
                if( VAR_SetStr( hVarServer, hVar, type, value ) == EOK )
                {
                    lua_pushnumber( L, 1 );
                    result = 1;
                }
                else
                {
                    lua_pushnil( L );
                }
            }
        }
        else
        {
            /* invalid variable handle */
            lua_pushnil( L );
        }
    }

    return result;
}

/*============================================================================*/
/*  var_find                                                                  */
/*!
    var.find()

    This var.find() function interfaces to the VAR_FindByName() function
    in the libvarserver.so library

    The name of the variable is passed in on the lua stack
    and the variable handle is pushed back onto the lua stack.
    If the variable is not found, then nil is pushed back onto the lua stack

    @param[in]
        L
            pointer to the lua state

    @return always returns 0

==============================================================================*/
static int var_find( lua_State *L )
{
    int result = 0;
    char *name;
    size_t len;
    VAR_HANDLE hVar;

    if( L != NULL )
    {
        name = (char *)luaL_checklstring( L, 1, &len );
        if( name != NULL )
        {
            hVar = VAR_FindByName( hVarServer, name );

            if( hVar != VAR_INVALID )
            {
                lua_pushnumber( L, hVar );
                result = 1;
            }
        }
    }

    if( result == 0 )
    {
        lua_pushnil( L );
    }

    return result;
}

/*============================================================================*/
/*  var_notify                                                                */
/*!
    var.notify()

    This var.notify() function interfaces to the VAR_Notify() function
    in the libvarserver.so library

    The variable handle to be notified on is passed in on the lua stack
    The type of notification being requested is passed in on the lua stack


    @param[in]
        L
            pointer to the lua state

    @return always returns 0

==============================================================================*/
static int var_notify( lua_State *L )
{
    int result = 0;
    char *name;
    size_t len;
    VAR_HANDLE hVar;
    NotificationType notificationType;

    if( L != NULL )
    {
        hVar = (VAR_HANDLE)luaL_checknumber( L, 1 );
        notificationType = (NotificationType)luaL_checknumber( L, 2 );

        result = VAR_Notify( hVarServer, hVar, notificationType );
        if( result == EOK )
        {
            lua_pushnumber( L, result );
        }
        else
        {
            lua_pushnil( L );
        }
    }

    return 1;
}

/*============================================================================*/
/*  var_wait                                                                  */
/*!
    var.wait()

    This var.wait() function waits for a variable notification signal

    When the signal is received the signal and payload ID are pushed
    onto the lua stack

    @param[in]
        L
            pointer to the lua state

    @return always returns 0

==============================================================================*/
static int var_wait( lua_State *L )
{
    int result = 0;
    sigset_t mask;
    siginfo_t info;
    int sig;

    if( L != NULL )
    {
        sigemptyset( &mask );
        /* timer notification */
        sigaddset( &mask, SIGRTMIN+5 );
        /* modified notification */
        sigaddset( &mask, SIG_VAR_MODIFIED );
        /* calc notification */
        sigaddset( &mask, SIG_VAR_CALC );
        /* validate notification */
        sigaddset( &mask, SIG_VAR_PRINT );
        /* print notification */
        sigaddset( &mask, SIG_VAR_VALIDATE );

        /* block on these signals */
        sigprocmask( SIG_BLOCK, &mask, NULL );

        /* wait for a signal */
        sig = sigwaitinfo( &mask, &info );
        lua_pushnumber( L, sig );
        lua_pushnumber( L, info._sifields._timer.si_sigval.sival_int );

        result = 2;
    }

    return result;
}

/*============================================================================*/
/*  var_validate_start                                                        */
/*!
    var.validate_start()

    This var.validate_start() function starts a validation on a variable.

    The validation identifier that is received via var.wait() is passed
    in as an argument on the lua stack.

    The validate_start() function calls the VAR_GetValidationRequest()
    in the variable server library.

    The variable handle and the variable value are passed back on the
    lua stack.

    @param[in]
        L
            pointer to the lua state

    @return always returns 0

==============================================================================*/
static int var_validate_start( lua_State *L )
{
    int result = 0;
    char buf[BUFSIZ];
    VarObject var;
    uint32_t id;
    VAR_HANDLE hVar;

    if( L != NULL )
    {
        id = luaL_checknumber( L, 1 );

        var.val.str = buf;
        var.len = BUFSIZ;

        if( VAR_GetValidationRequest( hVarServer,
                                      id,
                                      &hVar,
                                      &var ) == EOK )
        {
            lua_pushnumber( L, hVar );
            switch( var.type )
            {
                case VARTYPE_STR:
                    lua_pushstring( L, var.val.str );
                    result = 2;
                    break;

                case VARTYPE_INT16:
                    lua_pushnumber( L, var.val.i );
                    result = 2;
                    break;

                case VARTYPE_UINT16:
                    lua_pushnumber( L, var.val.ui );
                    result = 2;
                    break;

                case VARTYPE_INT32:
                    lua_pushnumber( L, var.val.l );
                    result = 2;
                    break;

                case VARTYPE_UINT32:
                    lua_pushnumber( L, var.val.ul );
                    result = 2;
                    break;

                case VARTYPE_INT64:
                    lua_pushnumber( L, var.val.ll );
                    result = 2;
                    break;

                case VARTYPE_UINT64:
                    lua_pushnumber( L, var.val.ull );
                    result = 2;
                    break;

                case VARTYPE_FLOAT:
                    lua_pushnumber( L, var.val.f );
                    result = 2;
                    break;

                default:
                    break;
            }
        }
    }

    return result;
}

/*================================================--==========================*/
/*  var_validate_end                                                          */
/*!
    var.validate_end()

    This var.validate_end() function completes a validation on a variable.

    The validation identifier that is received via var.wait() is passed
    in as an argument on the lua stack.

    The result indicating if the validation is successful is also passed
    as an argument on the lua stack.

    The validate_end() function calls the VAR_SendValidationResponse()
    in the variable server library.

    @param[in]
        L
            pointer to the lua state

    @return always returns 0

==============================================================================*/
static int var_validate_end( lua_State *L )
{
    uint32_t id;
    uint32_t response;

    id = luaL_checknumber( L, 1 );
    response = luaL_checknumber( L, 2 );

    if( L != NULL )
    {
        if( VAR_SendValidationResponse( hVarServer, id, response ) == EOK )
        {
            lua_pushnumber( L, 1 );
        }
        else
        {
            lua_pushnil( L );
        }
    }

    return 1;
}

/*============================================================================*/
/*  var_open_print_session                                                    */
/*!
    var.open_print_session()

    This var.open_print_session() function sets up a print session
    to be used to render variable strings.

    The session id is passed as the first argument on the lua stack.

    The function creates a new LuaPrintSession user data object
    starting with a luaL_stream object which contains the
    file pointer for the output stream.

    The LuaPrintSession object also includes the variable handle
    and the session identifier.

    If the print session was successfully opened, the function
    returns the LuaPrintSession object (aka luaL_stream) and the
    handle of the system variable to render.

    If the print session could not be successfully opened,
    the function returns nil.

    @param[in]
        L
            pointer to the lua state

    @return the number of arguments returned on the Lua stack

==============================================================================*/
static int var_open_print_session( lua_State *L )
{
    LuaPrintSession *pLuaPrintSession;
    uint32_t id;
    VAR_HANDLE hVar;
    FILE *fp;
    int fd;
    int result = 0;

    id = luaL_checknumber( L, 1 );

    if ( VAR_OpenPrintSession( hVarServer, id, &hVar, &fd ) == EOK )
    {
        pLuaPrintSession = (LuaPrintSession *)
                            lua_newuserdata ( L, sizeof( LuaPrintSession ));
        if( pLuaPrintSession != NULL )
        {
            luaL_setmetatable( L, LUA_FILEHANDLE );

            pLuaPrintSession->id = id;
            pLuaPrintSession->fd = fd;
            fp = fdopen( fd, "w" );
            pLuaPrintSession->hVar = hVar;
            pLuaPrintSession->stream.f = fp;
            pLuaPrintSession->stream.closef = &var_close_print_session;

            lua_pushnumber( L, hVar );

            result = 2;
        }
    }
    else
    {
        lua_pushnil( L );
        result = 1;
    }

    return result;
}


/*============================================================================*/
/*  var_close_print_session                                                   */
/*!
    var.close_print_session()

    This var.close_print_session() function shuts down a print session
    that was used to render variable strings.

    The Lua Print Session object containing the luaL_Stream
    is passed as the first argument on the lua stack.

    The function closes the print session using the
    VAR_ClosePrintSession function and clears the
    LuaPrintSession object.

    On success this function pushes 1 onto the Lua stack

    On failure this function pushes nil and the failure error string

    @param[in]
        L
            pointer to the lua state

    @return the numbner of arguments returned on the Lua stack

==============================================================================*/
static int var_close_print_session( lua_State *L )
{
    LuaPrintSession *pLuaPrintSession;
    int result = 0;

    if( L != NULL )
    {
        pLuaPrintSession = (LuaPrintSession *)
                        luaL_checkudata( L, 1, LUA_FILEHANDLE );
        if( pLuaPrintSession != NULL )
        {
            result = VAR_ClosePrintSession( hVarServer,
                                            pLuaPrintSession->id,
                                            pLuaPrintSession->fd );

            fclose( pLuaPrintSession->stream.f );

            memset( pLuaPrintSession, 0, sizeof( LuaPrintSession ) );
        }

        if( result == EOK )
        {
            lua_pushnumber( L, 1 );
            result = 1;
        }
        else
        {
            lua_pushnil( L );
            lua_pushstring( L, strerror( result ) );
            result = 2;
        }
    }

    return result;
}

/*! @}
 * end of libluavars group */
