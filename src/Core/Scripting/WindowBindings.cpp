#include "WindowBindings.h"
#include "LuaBinding.h"
#include "IWindow.h"

extern "C" {
#include <lauxlib.h>
}

namespace {

constexpr const char* kMetatableName = "Coffee.IWindow";

int Lua_GetWidth(lua_State* L) {
    lua_pushinteger(L, LuaBinding::CheckPtr<IWindow>(L, 1, kMetatableName)->GetWidth());
    return 1;
}

int Lua_GetHeight(lua_State* L) {
    lua_pushinteger(L, LuaBinding::CheckPtr<IWindow>(L, 1, kMetatableName)->GetHeight());
    return 1;
}

int Lua_SetIcon(lua_State* L) {
    IWindow* window = LuaBinding::CheckPtr<IWindow>(L, 1, kMetatableName);
    const char* filepath = luaL_checkstring(L, 2);
    window->SetIcon(filepath);
    return 0;
}

const luaL_Reg kMethods[] = {
    {"GetWidth", Lua_GetWidth},
    {"GetHeight", Lua_GetHeight},
    {"SetIcon", Lua_SetIcon},
    {nullptr, nullptr}
};

} //  End of Namespace

void WindowBindings::Register(lua_State* L, IWindow* window) {
    LuaBinding::RegisterMetatable<IWindow>(L, kMetatableName, kMethods);
    LuaBinding::PushPtr<IWindow>(L, kMetatableName, window);
    lua_setglobal(L, "eWindow");
}