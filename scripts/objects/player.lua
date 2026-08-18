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

    -- Solid white generated sprite instead of a flat-color quad -- makes
    -- the player pixel-addressable too, so lighting (a torch flickering
    -- across the player as they walk by) actually shows up on them.
    self.sprite = Sprite.NewSolid(w, h, 1.0, 1.0, 1.0, 1.0)
    self.body:SetSprite(self.sprite)

    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2))

    self.config = PlayerActorConfig.new()
    self.config:SetMoveSpeed(75)
    self.config:SetJumpForce(200)
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
--
-- worldWidth/worldHeight: the play area's bounds IN TEXELS (world
-- units) -- e.g. Constants.RESOLUTION_WIDTH/HEIGHT for a level that fits
-- entirely within one camera frame. Deliberately NOT eWindow:GetWidth()/
-- GetHeight() -- those are REAL window/monitor pixels, a completely
-- different number from the texel grid every body's position/size is
-- authored in the moment a Camera2D with its own viewportSize is in
-- play (see Camera2D.h). Falls back to the real window size only if the
-- caller doesn't pass anything, so this stays harmless for a script that
-- never sets up a camera at all (the old "world pixels == window
-- pixels" identity-mapping default -- see Renderer2D::ApplyCommonUniforms).
function Player:Update(deltaTime, solids, worldWidth, worldHeight)
    self:HandleInput()
    self.body:Integrate(deltaTime)

    self.body:ResolveWindowBounds(worldWidth or eWindow:GetWidth(), worldHeight or eWindow:GetHeight())
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