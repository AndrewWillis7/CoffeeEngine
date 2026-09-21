-- The player-controlled Character: keyboard input to velocity and gait state.
-- Body construction and the per-frame update/draw loop live on Character; this
-- file is only input handling.

local Class = require("core.Class")
local Character = require("objects.character")

local Player = Class(Character)

function Player.new(x, y, w, h, legConfig, torsoConfig)
    local self = Character.new(x, y, w, h, legConfig, torsoConfig)
    setmetatable(self, Player)
    self.body:SetName("Player")

    -- Multipliers on the config's walk speed. Held per player rather than as
    -- literals in HandleInput so the scene editor can tune them alongside it.
    self.sprintSpeed = 2.2
    self.crouchSpeed = 0.5
    self.body:Expose("speed.sprint", self, "sprintSpeed", { min = 1, max = 6, step = 0.01 })
    self.body:Expose("speed.crouch", self, "crouchSpeed", { min = 0.05, max = 1, step = 0.005 })
    return self
end

-- Split out from Update() so a pause menu or cutscene can disable just this
-- part while physics and drawing keep running.
function Player:HandleInput()
    if not self.config:IsInputEnabled() then return end

    local dx = 0
    if Input.IsKeyDown(Keys.A) then dx = dx - 1 end
    if Input.IsKeyDown(Keys.D) then dx = dx + 1 end

    -- Hold to activate, not toggles. Crouch wins if both are held.
    local crouching = Input.IsKeyDown(Keys.Ctrl)
    local sprinting = (not crouching) and Input.IsKeyDown(Keys.Shift)

    -- Drives the leg rig's stride and step height: sprinting takes longer,
    -- higher steps, crouching shorter ones with sunken hips.
    self:SetGaitState(crouching and "crouch" or (sprinting and "sprint" or "walk"))

    local speedScale = crouching and self.crouchSpeed or (sprinting and self.sprintSpeed or 1.0)
    local speed = self.config:GetMoveSpeed() * speedScale

    local _, vy = self.body:GetVelocity()

    -- Jumping is Lua-side; gravity in RigidBody2D::Integrate brings vy back
    -- down. IsGrounded() reflects last frame's landing check, which is exactly
    -- the "can I jump right now" question. No crouch-jumping.
    if not crouching and Input.IsKeyPressed(Keys.Space) and self.body:IsGrounded() then
        vy = -self.config:GetJumpForce()
    end

    self.body:SetVelocity(dx * speed, vy)
end

return Player
