#pragma once
#include <string>

struct lua_State;

namespace KeyMap {

void LoadAndExposeToLua(lua_State* L, const std::string& path);
void SyncFromLua(lua_State* L);
int Get(const std::string& name);

} // End of Namespace KeyMap