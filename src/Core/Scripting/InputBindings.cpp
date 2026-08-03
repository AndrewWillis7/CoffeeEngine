#include "InputBindings.h"
#include "Core/Input/UserInputService.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

int Lua_IsKeyDown(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    int keycode = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, input && input->IsKeyDown(keycode));
    return 1;
}

int Lua_IsKeyPressed(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    int keycode = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, input && input->IsKeyPressed(keycode));
    return 1;
}

int Lua_IsKeyReleased(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    int keycode = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, input && input->IsKeyReleased(keycode));
    return 1;
}

// Mouse buttons cross the Lua boundary as small integers -- see the
// Input.MouseLeft / MouseRight / MouseMiddle constants registered below
// (they mirror the MouseButton enum in IWindow.h).
int Lua_IsMouseButtonDown(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    int button = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, input && input->IsMouseButtonDown(static_cast<MouseButton>(button)));
    return 1;
}

int Lua_IsMouseButtonPressed(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    int button = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, input && input->IsMouseButtonPressed(static_cast<MouseButton>(button)));
    return 1;
}

int Lua_IsMouseButtonReleased(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    int button = static_cast<int>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, input && input->IsMouseButtonReleased(static_cast<MouseButton>(button)));
    return 1;
}

int Lua_GetMousePosition(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    Vector2 pos = input ? input->GetMousePosition() : Vector2::Zero();
    lua_pushnumber(L, pos.x);
    lua_pushnumber(L, pos.y);
    return 2;
}

// Input.GetKeysPressedThisFrame() -- returns an array table of every
// keycode that went down this frame. Mainly for figuring out what a key's
// raw code is on your machine: hold it and print the table's contents.
int Lua_GetKeysPressedThisFrame(lua_State* L) {
    auto* input = static_cast<UserInputService*>(lua_touserdata(L, lua_upvalueindex(1)));
    lua_newtable(L);
    if (!input) return 1;

    int i = 1;
    for (int keycode : input->GetKeysPressedThisFrame()) {
        lua_pushinteger(L, keycode);
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

// Small helper so registering seven near-identical closures doesn't turn
// into seven copies of the same three lines.
void PushFunc(lua_State* L, UserInputService* input, lua_CFunction fn, const char* name) {
    lua_pushlightuserdata(L, input);
    lua_pushcclosure(L, fn, 1);
    lua_setfield(L, -2, name);
}

} // namespace

void InputBindings::Register(lua_State* L, UserInputService* input) {
    lua_newtable(L);

    PushFunc(L, input, Lua_IsKeyDown, "IsKeyDown");
    PushFunc(L, input, Lua_IsKeyPressed, "IsKeyPressed");
    PushFunc(L, input, Lua_IsKeyReleased, "IsKeyReleased");
    PushFunc(L, input, Lua_IsMouseButtonDown, "IsMouseButtonDown");
    PushFunc(L, input, Lua_IsMouseButtonPressed, "IsMouseButtonPressed");
    PushFunc(L, input, Lua_IsMouseButtonReleased, "IsMouseButtonReleased");
    PushFunc(L, input, Lua_GetMousePosition, "GetMousePosition");
    PushFunc(L, input, Lua_GetKeysPressedThisFrame, "GetKeysPressedThisFrame");

    // Named constants so scripts can write Input.IsMouseButtonDown(Input.MouseLeft)
    // instead of a magic number.
    lua_pushinteger(L, static_cast<int>(MouseButton::Left));
    lua_setfield(L, -2, "MouseLeft");
    lua_pushinteger(L, static_cast<int>(MouseButton::Right));
    lua_setfield(L, -2, "MouseRight");
    lua_pushinteger(L, static_cast<int>(MouseButton::Middle));
    lua_setfield(L, -2, "MouseMiddle");

    lua_setglobal(L, "Input");
}