-- A non-controllable Character: instead of reading keyboard input, it
-- randomly picks a direction (left, right, or stand still) and holds
-- it for a random duration before picking again. Same body/leg-rig
-- plumbing as the player by way of Character -- an NPC IS a Character,
-- just driven by this tiny AI instead of Input.*. Deliberately simple
-- for now (see class header) -- a real state machine (idle/patrol/
-- flee/etc.) can replace HandleInput later without touching Character.

local Class = require("core.Class")
local Character = require("objects.character")

local NPC = Class(Character)

NPC.WALK_SPEED = 18
NPC.MIN_HOLD, NPC.MAX_HOLD = 1.0, 3.0

function NPC.new(x, y, w, h, legConfig, torsoConfig)
    local self = Character.new(x, y, w, h, legConfig, torsoConfig)
    setmetatable(self, NPC)

    self.walkDir = 0
    self.holdTime = 0
    return self
end

-- Picks a new direction (-1, 0, 1) and how long to hold it before the
-- next pick.
function NPC:PickNewDirection()
    local r = math.random(3)
    self.walkDir = (r == 1) and -1 or (r == 2) and 1 or 0
    self.holdTime = NPC.MIN_HOLD + math.random() * (NPC.MAX_HOLD - NPC.MIN_HOLD)
end

function NPC:HandleInput(deltaTime)
    self.holdTime = self.holdTime - (deltaTime or 0)
    if self.holdTime <= 0 then
        self:PickNewDirection()
    end

    local _, vy = self.body:GetVelocity()
    self.body:SetVelocity(self.walkDir * NPC.WALK_SPEED, vy)
end

return NPC
