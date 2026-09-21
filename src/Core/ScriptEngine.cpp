#include "ScriptEngine.h"
#include "EngineContext.h"
#include "ActorRegistry.h"
#include "Gameplay/LightingSystem.h"
#include "Scripting/ScriptBindings.h"
#include "Input/KeyMap.h"

#include <iostream>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace {

// Replaces Lua's own print. Stock print writes to stdout with fwrite, which
// slips straight past the std::cout tee DebugLog installs, so script output
// would never reach the in-engine log. Formatting matches Lua 5.4: every
// argument through tostring, tab separated, one trailing newline.
int Lua_Print(lua_State* L) {
    const int count = lua_gettop(L);
    std::string line;
    for (int i = 1; i <= count; ++i) {
        size_t length = 0;
        const char* text = luaL_tolstring(L, i, &length);
        if (i > 1) line += '\t';
        line.append(text, length);
        lua_pop(L, 1); // luaL_tolstring pushed its result
    }
    std::cout << line << "\n";
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
        std::cerr << "Engine Warning: Lua error in " << name << "(): "
                    << lua_tostring(L, -1) << "\n";
        lua_pop(L, 1);
    }
}

} // namespace

ScriptEngine::ScriptEngine() = default;
ScriptEngine::~ScriptEngine() {Shutdown();}

void ScriptEngine::Init(const std::string& scriptPath, EngineContext& context) {
    m_ScriptPath = scriptPath;
    m_Context = &context;
    
    m_Lua = luaL_newstate();
    luaL_openlibs(m_Lua);

    lua_pushcfunction(m_Lua, &Lua_Print);
    lua_setglobal(m_Lua, "print");

    // Prepend scripts/ to package.path so require("objects.Player") resolves to
    // scripts/objects/Player.lua, keeping the stock search locations as fallback.
    lua_getglobal(m_Lua, "package");
    lua_getfield(m_Lua, -1, "path");
    std::string newPath = std::string("scripts/?.lua;scripts/?/init.lua;") + lua_tostring(m_Lua, -1);
    lua_pop(m_Lua, 1);
    lua_pushstring(m_Lua, newPath.c_str());
    lua_setfield(m_Lua, -2, "path");
    lua_pop(m_Lua, 1);

    KeyMap::LoadAndExposeToLua(m_Lua, "scripts/keycodes.lua");

    ScriptBindings::RegisterAll(m_Lua, context);

    if (luaL_dofile(m_Lua, scriptPath.c_str()) != LUA_OK) {
        std::cerr << "Engine Fatal: failed to load " << scriptPath << ": "
                    << lua_tostring(m_Lua, -1) << "\n";
        lua_pop(m_Lua, 1);
        return;
    }

    CallIfExists(m_Lua, "Init", 0);
    KeyMap::SyncFromLua(m_Lua);

    if (m_OnLoaded) m_OnLoaded();
}

bool ScriptEngine::CallSpawn(const std::string& fnName, float x, float y, const std::string& name) {
    if (!m_Lua) return false;

    lua_getglobal(m_Lua, fnName.c_str());
    if (!lua_isfunction(m_Lua, -1)) {
        lua_pop(m_Lua, 1);
        std::cerr << "Engine Warning: no Lua function '" << fnName << "' to spawn '" << name << "' with\n";
        return false;
    }

    lua_pushnumber(m_Lua, x);
    lua_pushnumber(m_Lua, y);
    lua_pushstring(m_Lua, name.c_str());
    if (lua_pcall(m_Lua, 3, 0, 0) != LUA_OK) {
        std::cerr << "Engine Warning: Lua error in " << fnName << "(): " << lua_tostring(m_Lua, -1) << "\n";
        lua_pop(m_Lua, 1);
        return false;
    }
    return true;
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

void ScriptEngine::Reload() {
    if (!m_Context) {
        std::cerr << "Engine Warning: ScriptEngine::Reload() called before Init()\n";
        return;
    }
    std::cout << "Reloading Lua scripts...\n";

    Shutdown();

    // Order matters: LightingSystem caches raw RigidBody2D* from last frame, so it
    // must drop them before actors->Clear() frees those bodies.
    if (m_Context->lighting) {
        m_Context->lighting->Reset();
    }
    if (m_Context->actors) {
        m_Context->actors->Clear();
    }
    Init(m_ScriptPath, *m_Context);
    ++m_ReloadCount;
}