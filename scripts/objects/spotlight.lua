-- Like Campfire, but a fixed-direction Cone instead of a flickering Point and
-- with no sprite or Draw() at all -- a wall-mounted fixture that stays where it
-- is pointed, rather than a light riding along with a visible body.

local Class = require("core.Class")

local Spotlight = Class()

---@param x number
---@param y number
---@param aimDegrees number Absolute world aim in degrees: 0 = right, 90 = down, 180 = left.
function Spotlight.new(x, y, aimDegrees)
    local self = setmetatable({}, Spotlight)

    self.body = RigidBody2D.new(x, y)

    self.light = LightEmitterConfig.new()
    self.light:SetType("Cone")
    self.light:SetColor(0.55, 0.25, 0.9, 1.0) -- cool violet/purple
    self.light:SetRadius(300)
    self.light:SetBrightness(1.3)
    -- Softer than the default 2.0: this is far from what it lights, and a tight
    -- hotspot falloff would be barely there by the time it arrives.
    self.light:SetFalloffExponent(1.5)
    self.light:SetConeAngle(70)
    self.light:SetConeDirection(aimDegrees)

    -- Bolted in place: an absolute world aim, not one that turns with the
    -- owning body's rotation.
    self.light:SetUseOwnerRotation(false)

    self.body:SetLightEmitter(self.light)

    return self
end

return Spotlight