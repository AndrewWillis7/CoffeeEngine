-- Minimal single-inheritance class helper -- hand-rolled, not a third-party
-- OOP library, same "raw and explicit over a framework" spirit as the
-- engine's own C++ Lua bindings (LuaBinding.h ptr/value userdata pattern
-- instead of sol2). Every game object module (Player, and whatever follows
-- it) shares this so they all look and behave the same way.
--
-- Usage, no inheritance:
--   local Player = Class()
--   function Player.new(x, y)
--       local self = setmetatable({}, Player)
--       self.x, self.y = x, y
--       return self
--   end
--   function Player:Update(dt) ... end
--
-- Usage, with inheritance:
--   local Enemy = Class(Actor)  -- Enemy now falls back to Actor's methods
--   function Enemy.new(...)
--       local self = Actor.new(...)      -- build the base part
--       return setmetatable(self, Enemy) -- then re-tag it as an Enemy
--   end

local function Class(base)
    local cls = {}
    cls.__index = cls
    if base then
        setmetatable(cls, {__index = base})
    end
    return cls
end

return Class