-- Wraps a RigidBody2D + LightEmitterConfig as a single game object, same
-- pattern Campfire.lua uses -- but a fixed-direction Cone instead of a
-- flickering Point, and with no sprite/Draw() at all (this thing is
-- never drawn, same "just carries a component around" shape Camera.lua
-- already uses -- see its header comment). A wall/ceiling-mounted
-- fixture: pick a spot and an absolute world-space aim direction and it
-- stays there, unlike Campfire's light which rides along with a visible
-- body.

local Class = require("core.Class")

local Spotlight = Class()

---@param x number
---@param y number
---@param aimDegrees number Absolute world-space aim direction in degrees -- 0 = +X (right), 90 = +Y (down, since +y is down), 180 = -X (left). See Transform2D.h's rotation convention.
function Spotlight.new(x, y, aimDegrees)
    local self = setmetatable({}, Spotlight)

    self.body = RigidBody2D.new(x, y)

    self.light = LightEmitterConfig.new()
    self.light:SetType("Cone")
    self.light:SetColor(0.55, 0.25, 0.9, 1.0) -- cool violet/purple
    self.light:SetRadius(300)
    self.light:SetBrightness(1.3)
    self.light:SetFalloffExponent(1.5) -- softer than the default 2.0 -- this thing is far from what it's lighting, a tight hotspot falloff would read as barely-there by the time it reaches anything
    self.light:SetConeAngle(70)
    self.light:SetConeDirection(aimDegrees)

    -- Bolted in place, not mounted on anything that rotates -- an
    -- absolute world-space aim rather than one that'd turn with the
    -- (stationary) owning body's own rotation. See LightEmitterConfig.h's
    -- useOwnerRotation comment.
    self.light:SetUseOwnerRotation(false)

    self.body:SetLightEmitter(self.light)

    return self
end

return Spotlight