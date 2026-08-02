#include "ScriptEngine.h"
#include "EngineContext.h"
#include "Scripting/GraphicsBindings.h"
#include "Scripting/WindowBindings.h"

#include <iostream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace {

void CallIfExists(lua_State* L, const char* name, int nargs) {
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1 + nargs);
        return;
    }
    lua_insert(L, -(nargs + 1));
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
        std::cerr << "Engine Warning: Lua error in " << name << "(): "
                    << lua_tostring(L, -1) << "\n";
        lua_pop(L, 1);
    }
}

} // namespace

ScriptEngine::ScriptEngine() = default;
ScriptEngine::~ScriptEngine() {Shutdown();}

void ScriptEngine::Init(const std::string& scriptPath, EngineContext& context) {
    m_Lua = luaL_newstate();
    luaL_openlibs(m_Lua);

    // Binding Engine Subsystems
    GraphicsBindings::Register(m_Lua, context.graphics);
    WindowBindings::Register(m_Lua, context.window);
    // Future Actor Bindings
    // Future Sprite Bindings

    if (luaL_dofile(m_Lua, scriptPath.c_str()) != LUA_OK) {
        std::cerr << "Engine Fata: Failed to Load " << scriptPath << ": "
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