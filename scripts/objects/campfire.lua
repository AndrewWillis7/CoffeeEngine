-- Wraps a RigidBody2D + LightEmitterConfig as a single game object, same
-- pattern Camera.lua uses for RigidBody2D + Camera2D. A small solid
-- ember-colored square that emits a flickering orange/yellow point
-- light -- the concrete "campfire" starting point for the lighting
-- system (see Core/Gameplay/LightingSystem.h).

local Class = require("core.Class")

local Campfire = Class()

---@param x number
---@param y number
---@param size number|nil Defaults to 16
function Campfire.new(x, y, size)
    size = size or 16

    local self = setmetatable({}, Campfire)

    self.body = RigidBody2D.new(x, y, size, size)
    self.sprite = Sprite.NewSolid(size, size, 0.55, 0.15, 0.05, 1.0)
    self.body:SetSprite(self.sprite)

    self.light = LightEmitterConfig.new()
    self.light:SetType("Point")
    self.light:SetColor(1.0, 0.55, 0.15, 1.0) -- warm orange
    self.light:SetRadius(140)
    self.light:SetBrightness(1.1)
    self.light:SetFalloffExponent(2.0)

    self.light:SetFlicker(true)
    self.light:SetFlickerSpeed(7)
    self.light:SetFlickerIntensityAmount(0.3)
    self.light:SetFlickerColorShift(0.3, 0.25, 0.0, 0.0) -- shifts toward yellow at the flicker's peak

    self.body:SetLightEmitter(self.light)

    return self
end

function Campfire:Draw()
    DrawBody(self.body)
end

return Campfire