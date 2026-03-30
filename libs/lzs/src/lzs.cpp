#define LIB_NAME "LZS"
#define MODULE_NAME "lzs"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <iostream>
#include "stdio.h"
#include <string.h> 
#include "lzs.h"

static int compress(lua_State* L)
{
    const char* file_path = luaL_checkstring(L, 1);
    
    FILE * fp;

    if ((fp = fopen(file_path, "rb")) == NULL) {
        printf("File open error");
        // assert(top == lua_gettop(L));
        return 0;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);  /* same as rewind(f); */

    if (fsize < 0) {
        fclose(fp);
        lua_pushboolean(L, 0);
        return 1;
    }

    size_t input_size = (size_t)fsize;
    size_t max_output_size = LZS_COMPRESSED_MAX(input_size);

    uint8_t *in_bytes = (uint8_t*)malloc(input_size);
    uint8_t *out_bytes = (uint8_t*)malloc(max_output_size);

    if ((input_size && !in_bytes) || !out_bytes) {
        fclose(fp);
        free(in_bytes);
        free(out_bytes);
        lua_pushboolean(L, 0);
        return 1;
    }

    if (input_size) {
        fread(in_bytes, 1, input_size, fp);
    }
    
    size_t c = lzs_compress(out_bytes, max_output_size, in_bytes, input_size);

    fclose(fp);
    
    if ((fp = fopen(file_path, "wb")) == NULL) {
        printf("File open error");
        free(in_bytes);
        free(out_bytes);
        // assert(top == lua_gettop(L));
        lua_pushboolean(L, 0);
        return 1;
    }

    fwrite(out_bytes, 1, c,fp);
    fclose(fp);
    
    free(in_bytes);
    free(out_bytes);

    lua_pushboolean(L, 1);
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"compress", compress},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeLZS(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeLZS(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeLZS(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeLZS(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}


// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// CheckClick is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(LZS, LIB_NAME, AppInitializeLZS, AppFinalizeLZS, InitializeLZS, 0, 0, FinalizeLZS)
