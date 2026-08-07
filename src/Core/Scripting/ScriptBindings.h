#pragma once

struct lua_State;
struct EngineContext;

// Registers every engine subsystem exposed to Lua. One call from
// ScriptEngine::Init() instead of one Register() call per bound type --
// see ScriptBindings.cpp for the actual per-type binding code, organized
// into clearly-labeled sections rather than one file per type.
namespace ScriptBindings {
    void RegisterAll(lua_State* L, EngineContext& context);
} // End of Namespace ScriptBindings