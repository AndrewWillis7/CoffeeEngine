-- Wraps a RigidBody2D + PlayerActorConfig as a single game object instead
-- of main.lua juggling both as loose globals. `body` and `config` are left
-- public (self.body / self.config) rather than re-wrapped behind getters --
-- anything that needs raw position/color/velocity access still talks to
-- the RigidBody2D directly, this class only owns PLAYER-specific behavior
-- (movement + jump).
--
-- SetPlayerConfig() is still called during construction so the existing
-- C++-side "who is the player" mechanism (Actors.GetPlayer(), the debug
-- UI, ActorRegistry::DumpTree()) keeps working exactly as before -- this
-- class sits ON TOP of that, it doesn't replace it.

local Class = require("core.Class")

local Player = Class()

function Player.new(x, y, w, h)
    w = w or 50
    h = h or 50

    local self = setmetatable({}, Player)

    self.body = RigidBody2D.new(x, y, w, h)
    self.body:SetColor(1.0, 1.0, 1.0, 1.0)
    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2))

    self.config = PlayerActorConfig.new()
    self.config:SetMoveSpeed(250)
    self.config:SetJumpForce(550)
    self.body:SetPlayerConfig(self.config)

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

    local _, vy = self.body:GetVelocity()

    -- Jump: Lua-side, no C++ Jump() function -- gravity (C++-applied
    -- inside RigidBody2D::Integrate()) is what brings vy back down.
    -- IsGrounded() reflects last frame's landing check, which is exactly
    -- the "can I jump right now" question.
    if Input.IsKeyPressed(Keys.Space) and self.body:IsGrounded() then
        vy = -self.config:GetJumpForce()
    end

    self.body:SetVelocity(dx * self.config:GetMoveSpeed(), vy)
end

-- solids: array of RigidBody2D to resolve collisions against this frame
-- (e.g. {floor, wall, crate}). Kept as a plain parameter rather than
-- something Player tracks itself -- which bodies count as "solid" is a
-- level concern, not a player concern.
function Player:Update(deltaTime, solids)
    self:HandleInput()
    self.body:Integrate(deltaTime)

    self.body:ResolveWindowBounds(eWindow:GetWidth(), eWindow:GetHeight())
    if solids then
        for _, solid in ipairs(solids) do
            self.body:ResolveCollisionWith(solid)
        end
    end
end

function Player:Draw()
    DrawBody(self.body)
end

return Player