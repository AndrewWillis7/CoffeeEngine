#pragma once
#include <string>

struct lua_State;
struct EngineContext;

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    void Init(const std::string& scriptPath, EngineContext& context);
    void Update(float deltaTime);
    void Shutdown();

private:
    lua_State* m_Lua = nullptr;
};