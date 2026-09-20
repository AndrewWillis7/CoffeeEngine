-- The player-controlled Character: reads keyboard input and turns it
-- into velocity + gait state every frame. All the actual body/leg
-- construction and the generic per-frame update/draw loop now live on
-- Character (objects/character.lua); this file is just input handling.

local Class = require("core.Class")
local Character = require("objects.character")

local Player = Class(Character)

function Player.new(x, y, w, h, legConfig, torsoConfig)
    local self = Character.new(x, y, w, h, legConfig, torsoConfig)
    setmetatable(self, Player)
    return self
end

-- Reads input and sets velocity for this frame. Split out from Update()
-- so a future pause menu/cutscene/death state can skip just this part
-- (via config:SetInputEnabled(false)) while everything else -- physics,
-- drawing -- keeps running.
function Player:HandleInput()
    if not self.config:IsInputEnabled() then return end

    local dx = 0
    if Input.IsKeyDown(Keys.A) then dx = dx - 1 end
    if Input.IsKeyDown(Keys.D) then dx = dx + 1 end

    -- Hold-to-activate, not toggles. Crouch wins if both are held --
    -- there's no such thing as a sprint-crouch.
    local crouching = Input.IsKeyDown(Keys.Ctrl)
    local sprinting = (not crouching) and Input.IsKeyDown(Keys.Shift)

    -- Drives the leg rig's stride/step-height/knee-bend tuning directly
    -- (see LegRig:SetGaitState) -- sprinting takes longer, higher steps,
    -- crouching takes shorter ones and sinks the hips.
    self:SetGaitState(crouching and "crouch" or (sprinting and "sprint" or "walk"))

    local speedScale = crouching and 0.5 or (sprinting and 2.2 or 1.0)
    local speed = self.config:GetMoveSpeed() * speedScale

    local _, vy = self.body:GetVelocity()

    -- Jump: Lua-side, no C++ Jump() function -- gravity (C++-applied
    -- inside RigidBody2D::Integrate()) is what brings vy back down.
    -- IsGrounded() reflects last frame's landing check, which is exactly
    -- the "can I jump right now" question. No crouch-jumping -- have to
    -- stand up first, same as most 2D platformers.
    if not crouching and Input.IsKeyPressed(Keys.Space) and self.body:IsGrounded() then
        vy = -self.config:GetJumpForce()
    end

    self.body:SetVelocity(dx * speed, vy)
end

return Player
