#pragma once
#include <functional>
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

    void Reload();

    // Calls a global Lua function of the shape `function(x, y, name)`, used by
    // the scene editor to spawn a persisted, editor-placed asset (see
    // scripts/core/spawn_registry.lua). Logs and returns false if the function
    // is missing or errors; never throws into the editor.
    bool CallSpawn(const std::string& fnName, float x, float y, const std::string& name);

    const std::string& GetScriptPath() const { return m_ScriptPath; }
    int GetReloadCount() const { return m_ReloadCount; }

    // Runs after every successful load -- Init() and each Reload() -- once the
    // script's own Init() has built the scene. The scene editor hangs off this
    // to lay its saved edits over whatever the scripts just created. Pass an
    // empty function to unhook.
    void SetOnLoaded(std::function<void()> callback) { m_OnLoaded = std::move(callback); }

private:
    lua_State* m_Lua = nullptr;
    std::string m_ScriptPath;
    EngineContext* m_Context = nullptr;
    int m_ReloadCount = 0;
    std::function<void()> m_OnLoaded;
};