#pragma once
#include <string>

struct lua_State;

// One field of a Lua table, offered to the scene editor by the script that
// owns it:
//
//   body:Expose("gait.stride", rig, "stride", { min = 8, max = 48 })
//
// Gameplay constants live in Lua, so this is how they reach the inspector
// and the .scene file without the engine knowing any of them by name. The
// editor lists them under the body that exposed them, edits and undoes them
// like any other field, and re-applies saved values after the scripts' Init().
//
// Holds a registry reference to the table, so it outlives the Lua local it
// came from. Both references die with the lua_State on reload -- at the same
// moment the body carrying this is freed, so neither is ever read dead.
struct ScriptTunable {
    enum class Kind { Number, Integer, Bool };

    std::string name;  // inspector label; the .scene key is "script." + name
    std::string key;   // field inside the table
    Kind kind = Kind::Number;

    // Inspector hints. min < max clamps every write; step is the value per
    // dragged pixel (0 derives one from the range).
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float step = 0.0f;
    int decimals = 2;

    lua_State* L = nullptr;
    int tableRef = -2;    // LUA_NOREF
    int onChangeRef = -2; // optional function(table) run after each write

    // False when the table no longer holds a number (or boolean) there.
    bool Read(float& out) const;

    // Clamps, writes, then calls onChange(table). A script error is logged and
    // swallowed: an inspector drag must never be what takes the frame down.
    void Write(float value) const;
};
