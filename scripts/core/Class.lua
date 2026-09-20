-- Minimal single-inheritance class helper, hand-rolled rather than a
-- third-party OOP library -- the same raw-and-explicit spirit as the engine's
-- own Lua bindings. Every game object module shares it, so they all look alike.
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