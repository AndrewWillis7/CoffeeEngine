#include "Vector2Bindings.h"
#include "LuaBinding.h"
#include "Core/Math/Vector2.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

constexpr const char* kMetatableName = "Coffee.Vector2";

Vector2* CheckSelf(lua_State* L, int index) {
    return LuaBinding::CheckValue<Vector2>(L, index, kMetatableName);
}

int Lua_New(lua_State* L) {
    float x = static_cast<float>(luaL_optnumber(L, 1, 0.0));
    float y = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    LuaBinding::PushNew<Vector2>(L, kMetatableName, Vector2(x, y));
    return 1;
}

int Lua_GetX(lua_State* L) { lua_pushnumber(L, CheckSelf(L, 1)->x); return 1; }
int Lua_GetY(lua_State* L) { lua_pushnumber(L, CheckSelf(L, 1)->y); return 1; }

int Lua_Set(lua_State* L) {
    Vector2* self = CheckSelf(L, 1);
    self->x = static_cast<float>(luaL_checknumber(L, 2));
    self->y = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

int Lua_Length(lua_State* L) { lua_pushnumber(L, CheckSelf(L, 1)->Length()); return 1; }
int Lua_LengthSquared(lua_State* L) { lua_pushnumber(L, CheckSelf(L, 1)->LengthSquared()); return 1; }

int Lua_Normalized(lua_State* L) {
    LuaBinding::PushNew<Vector2>(L, kMetatableName, CheckSelf(L, 1)->Normalized());
    return 1;
}

int Lua_Dot(lua_State* L) {
    lua_pushnumber(L, CheckSelf(L, 1)->Dot(*CheckSelf(L, 2)));
    return 1;
}

int Lua_Distance(lua_State* L) {
    lua_pushnumber(L, Vector2::Distance(*CheckSelf(L, 1), *CheckSelf(L, 2)));
    return 1;
}

int Lua_Add(lua_State* L) {
    LuaBinding::PushNew<Vector2>(L, kMetatableName, *CheckSelf(L, 1) + *CheckSelf(L, 2));
    return 1;
}

int Lua_Sub(lua_State* L) {
    LuaBinding::PushNew<Vector2>(L, kMetatableName, *CheckSelf(L, 1) - *CheckSelf(L, 2));
    return 1;
}

// Handles both `vec * number` and `number * vec` -- Lua calls __mul with
// whichever operand order was written, and only one side is guaranteed
// to be our userdata.
int Lua_Mul(lua_State* L) {
    bool firstIsVector = lua_isuserdata(L, 1);
    Vector2* vec = firstIsVector ? CheckSelf(L, 1) : CheckSelf(L, 2);
    float scalar = static_cast<float>(luaL_checknumber(L, firstIsVector ? 2 : 1));
    LuaBinding::PushNew<Vector2>(L, kMetatableName, *vec * scalar);
    return 1;
}

int Lua_Eq(lua_State* L) {
    lua_pushboolean(L, *CheckSelf(L, 1) == *CheckSelf(L, 2));
    return 1;
}

int Lua_ToString(lua_State* L) {
    Vector2* self = CheckSelf(L, 1);
    lua_pushfstring(L, "Vector2(%f, %f)", static_cast<double>(self->x), static_cast<double>(self->y));
    return 1;
}

const luaL_Reg kMethods[] = {
    {"GetX", Lua_GetX},
    {"GetY", Lua_GetY},
    {"Set", Lua_Set},
    {"Length", Lua_Length},
    {"LengthSquared", Lua_LengthSquared},
    {"Normalized", Lua_Normalized},
    {"Dot", Lua_Dot},
    {"Distance", Lua_Distance},
    {"__add", Lua_Add},
    {"__sub", Lua_Sub},
    {"__mul", Lua_Mul},
    {"__eq", Lua_Eq},
    {"__tostring", Lua_ToString},
    {nullptr, nullptr}
};

} // namespace

void Vector2Bindings::Register(lua_State* L) {
    LuaBinding::RegisterValueMetatable<Vector2>(L, kMetatableName, kMethods);

    lua_newtable(L);
    lua_pushcfunction(L, Lua_New);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Vector2");
}