#pragma once
#include <string>

// Forward Declarations only
class IGraphicsContext;
class IWindow;
struct lua_State;

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    void Init(const std::string& scriptPath, IGraphicsContext* graphics, IWindow* window);
    void Update(float deltaTime);
    void Shutdown();

private:
    lua_State* m_Lua = nullptr;
};