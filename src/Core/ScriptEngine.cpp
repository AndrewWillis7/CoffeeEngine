#include "ScriptEngine.h"
#include "EngineContext.h"
#include "ActorRegistry.h"
#include "Scripting/GraphicsBindings.h"
#include "Scripting/WindowBindings.h"
#include "Scripting/Vector2Bindings.h"
#include "Scripting/RigidBody2DBindings.h"
#include "Scripting/ShaderBindings.h"
#include "Scripting/RendererBindings.h"
#include "Scripting/CollisionShape2DBindings.h"
#include "Scripting/PlayerActorConfigBindings.h"
#include "Scripting/ActorRegistryBindings.h"
#include "Scripting/InputBindings.h"
#include "Scripting/PhysicsBindings.h"
#include "Input/KeyMap.h"

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
    m_ScriptPath = scriptPath;
    m_Context = &context;
    
    m_Lua = luaL_newstate();
    luaL_openlibs(m_Lua);

    // Make require() resolve against scripts/ as its root, so game code can
    // split into modules (require("objects.Player") -> scripts/objects/Player.lua)
    // instead of one monolithic main.lua. Prepended (not replaced) so the
    // stock search locations luaL_openlibs already set up still work too.
    // Engine-level infrastructure, same reasoning as KeyMap owning the
    // hardcoded "scripts/keycodes.lua" path below -- a game script
    // shouldn't have to remember to set this up itself.
    lua_getglobal(m_Lua, "package");
    lua_getfield(m_Lua, -1, "path");
    std::string newPath = std::string("scripts/?.lua;scripts/?/init.lua;") + lua_tostring(m_Lua, -1);
    lua_pop(m_Lua, 1);
    lua_pushstring(m_Lua, newPath.c_str());
    lua_setfield(m_Lua, -2, "path");
    lua_pop(m_Lua, 1); // pop package table

    KeyMap::LoadAndExposeToLua(m_Lua, "scripts/keycodes.lua");

    // Binding Engine Subsystems
    GraphicsBindings::Register(m_Lua, context.graphics);
    WindowBindings::Register(m_Lua, context.window);
    Vector2Bindings::Register(m_Lua);
    RigidBody2DBindings::Register(m_Lua, context.actors);
    ShaderBindings::Register(m_Lua, context.actors);
    RendererBindings::Register(m_Lua, context.renderer);
    CollisionShape2DBindings::Register(m_Lua, context.actors);
    PlayerActorConfigBindings::Register(m_Lua, context.actors);
    ActorRegistryBindings::Register(m_Lua, context.actors);
    InputBindings::Register(m_Lua, context.input);
    PhysicsBindings::Register(m_Lua);
    // Future Actor Bindings
    // Future Sprite Bindings

    if (luaL_dofile(m_Lua, scriptPath.c_str()) != LUA_OK) {
        std::cerr << "Engine Fata: Failed to Load " << scriptPath << ": "
                    << lua_tostring(m_Lua, -1) << "\n";
        lua_pop(m_Lua, 1);
        return;
    }

    CallIfExists(m_Lua, "Init", 0);
    KeyMap::SyncFromLua(m_Lua);
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
    if (m_Context->actors) {
        m_Context->actors->Clear();
    }
    Init(m_ScriptPath, *m_Context);
}