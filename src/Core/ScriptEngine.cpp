#include "ScriptEngine.h"
#include "IGraphicsContext.h"
#include "IWindow.h"

#include <iostream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace {
    // LUA trampoline for SetClearColor(r, g, b). Stashed here for OS Visibility
int Lua_SetClearColor(lua_State* L) {
    float r = static_cast<float>(luaL_checknumber(L, 1));
    float g = static_cast<float>(luaL_checknumber(L, 2));
    float b = static_cast<float>(luaL_checknumber(L, 3));

    auto* graphics = static_cast<IGraphicsContext*>(lua_touserdata(L, lua_upvalueindex(1)));
    if (graphics)
        graphics->SetClearColor(r, g, b);

    return 0;
}

void CallIfExists(lua_State* L, const char* name, int nargs) {
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1 + nargs);
        return;
    }
    lua_insert(L, -(nargs + 1));
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
        std::cerr << "Engine Warning: Lua errror in " << name << "(): "
                    << lua_tostring(L, -1) << "\n";
        lua_pop(L, 1);
    }
}

} // End Namespace

ScriptEngine::ScriptEngine() = default;
ScriptEngine::~ScriptEngine() {Shutdown();}

void ScriptEngine::Init(const std::string& scriptPath, IGraphicsContext* graphics, IWindow* /*window*/) {
    m_Lua = luaL_newstate();
    luaL_openlibs(m_Lua);

    // Bind Engine Functions
    lua_pushlightuserdata(m_Lua, graphics);
    lua_pushcclosure(m_Lua, Lua_SetClearColor, 1);
    lua_setglobal(m_Lua, "SetClearColor");

    if (luaL_dofile(m_Lua, scriptPath.c_str()) != LUA_OK) {
        std::cerr << "Engine Fatal: Failed to load " << scriptPath << ": "
                    << lua_tostring(m_Lua, -1) << "\n";
        lua_pop(m_Lua, 1);
        return;
    }

    CallIfExists(m_Lua, "Init", 0);
}

void ScriptEngine::Update(float deltaTime) {
    if (!m_Lua) return;
    lua_pushnumber(m_Lua, deltaTime);
    CallIfExists(m_Lua, "Update", 1);
}

void ScriptEngine::Shutdown() {
    if (m_Lua) {
        lua_close(m_Lua);
        m_Lua = nullptr;
    }
}