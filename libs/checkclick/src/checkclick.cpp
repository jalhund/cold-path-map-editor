#define LIB_NAME "CheckClick"
#define MODULE_NAME "checkclick"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <iostream>
#include <string.h> 

const static int MAX_IMAGE_COUNT = 1024;

struct ImageData {
    char* id;
    uint32_t size; 
    uint8_t* data; 
};

struct ImageList {
    int imageCount = 0;
    dmArray<ImageData> imageData;
} imageList;

static int clear(lua_State* L)
{
	for(int i = 0;i < imageList.imageCount;++i)
	{
		delete[] imageList.imageData[i].id;
		delete[] imageList.imageData[i].data;
	}
    imageList.imageCount = 0;
    imageList.imageData.SetSize(0);
    imageList.imageData.SetCapacity(MAX_IMAGE_COUNT);
    return 0;
}

static int load_generated_data_from_file(lua_State* L)
{
    char* id = (char*)luaL_checkstring(L, 1);
    char* file_path = (char*)luaL_checkstring(L, 2);
    int size = lua_tointeger(L, 3);
    
    FILE * fp;

    if ((fp = fopen(file_path, "rb")) == NULL) {
        printf("File open error");
        // assert(top == lua_gettop(L));
        return 0;
    }

    ImageData imageData;
    imageData.id = new char[strlen(id) + 1];
    strncpy(imageData.id, id, strlen(id) + 1);
    imageData.size = size;
    imageData.data = new uint8_t[size];

    int c = fread(imageData.data, 1, size, fp);

    fclose(fp);

    imageList.imageCount++;
    imageList.imageData.Push(imageData);

    lua_pushnumber(L, 1);
    return 1;
}

static int init(lua_State* L)
{
    char* id = (char*)luaL_checkstring(L, 1);
    char* str = (char*)luaL_checkstring(L, 2);
    
    int len = strlen(str);


    ImageData imageData;
    imageData.id = new char[strlen(id) + 1];
    strncpy(imageData.id, id, strlen(id) + 1);
    imageData.size = len;
    imageData.data = new uint8_t[len - 1];

    int compression = str[0] - '0';

    for(uint32_t i = 0;i < len - 1;++i)
        imageData.data[i] = str[i + 1] - '0';

    imageList.imageCount++;
    imageList.imageData.Push(imageData);

    lua_pushnumber(L, compression);
    return 1;
}


static int checkaplha(lua_State* L)
{
    char* id = (char*)luaL_checkstring(L, 1);
    uint32_t i = lua_tointeger(L, 2);

    int image_n = 0;
    for (int i = 0; i < imageList.imageData.Size(); ++i)
        if(!strcmp(imageList.imageData[i].id, id))
            image_n = i;
    if(i >= 0)
        lua_pushboolean(L, imageList.imageData[image_n].data[i]);
    else
        lua_pushboolean(L, 0);
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"clear", clear},
    {"load_from_file", load_generated_data_from_file},
    {"init", init},
    {"checkaplha", checkaplha},
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

dmExtension::Result AppInitializeCheckClick(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeCheckClick(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeCheckClick(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeCheckClick(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}


// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// CheckClick is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(CheckClick, LIB_NAME, AppInitializeCheckClick, AppFinalizeCheckClick, InitializeCheckClick, 0, 0, FinalizeCheckClick)
