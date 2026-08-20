-- Wraps a RigidBody2D + PlayerActorConfig as a single game object instead
-- of main.lua juggling both as loose globals. `body` and `config` are left
-- public (self.body / self.config) rather than re-wrapped behind getters --
-- anything that needs raw position/color/velocity access still talks to
-- the RigidBody2D directly, this class only owns PLAYER-specific behavior
-- (movement + jump).
--
-- Inherits LegRig (objects/leg_rig.lua), so the player IS a legged actor:
-- everything an NPC would use to walk -- module sizes/colors, the knee
-- joint, ground snapping, the gait -- is the same code path, configured
-- through the same table.
--
-- SetPlayerConfig() is still called during construction so the existing
-- C++-side "who is the player" mechanism (Actors.GetPlayer(), the debug
-- UI, ActorRegistry::DumpTree()) keeps working exactly as before.

local Class = require("core.Class")
local LegRig = require("objects.leg_rig")

local Player = Class(LegRig)

-- h is the TOTAL character height (torso + legs), same meaning it had
-- before legs existed -- a 16x32 player is still 16x32 from the crown of
-- its head to the soles of its boots, and still collides as one box that
-- size. What changed is that the bottom LegRig:GetStandHeight() texels of
-- that box are now legs rather than torso:
--
--   body position ---> +---------+  <- torso sprite (w x torsoHeight)
--                      |         |
--                      +---------+  <- hips
--                         | |        legs (standHeight)
--                         o o     <- collider bottom == where feet land
--
-- The collider is authored with an OFFSET (CollisionShape2D.NewBox's
-- 3rd/4th args) rather than by moving the body, because the body's
-- position has to stay the TORSO'S center -- that's the point DrawBody
-- draws the torso sprite around, and the point the camera follows.
--
-- legConfig is forwarded straight to LegRig (see its Defaults table).
-- Pass `false` for a legless player -- the torso then fills the full
-- height and every leg call becomes a no-op.
function Player.new(x, y, w, h, legConfig)
    w = w or 50
    h = h or 50

    -- Base part first, then re-tag -- the inheritance pattern documented
    -- in core/Class.lua. The rig has to exist before the torso can be
    -- sized, because GetStandHeight() is what decides how much of `h` is
    -- leg rather than torso.
    local self = LegRig.new(legConfig)
    setmetatable(self, Player)

    local standHeight = math.min(self:GetStandHeight(), h - 1)
    local torsoHeight = math.max(1, math.floor(h - standHeight + 0.5))
    standHeight = h - torsoHeight

    self.body = RigidBody2D.new(x, y, w, torsoHeight)

    -- Solid white generated sprite instead of a flat-color quad -- makes
    -- the player pixel-addressable too, so lighting (a torch flickering
    -- across the player as they walk by) actually shows up on them.
    self.sprite = Sprite.NewSolid(w, torsoHeight, 1.0, 1.0, 1.0, 1.0)
    self.body:SetSprite(self.sprite)

    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2, 0, standHeight / 2))

    self.config = PlayerActorConfig.new()
    self.config:SetMoveSpeed(75)
    self.config:SetJumpForce(200)
    self.body:SetPlayerConfig(self.config)

    -- Hips at the torso's bottom edge; feet then land exactly on the
    -- collider's bottom edge, which is what's resting on the floor.
    self:SetOwner(self.body, torsoHeight / 2)

    self.torsoHeight = torsoHeight
    self.height = h

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
-- level concern, not a player concern. The SAME list doubles as what the
-- leg rig ground-snaps against, so feet and collider always agree on
-- what the floor is.
--
-- worldWidth/worldHeight: the play area's bounds IN TEXELS (world
-- units) -- e.g. Constants.RESOLUTION_WIDTH/HEIGHT for a level that fits
-- entirely within one camera frame. Deliberately NOT eWindow:GetWidth()/
-- GetHeight() -- those are REAL window/monitor pixels, a completely
-- different number from the texel grid every body's position/size is
-- authored in the moment a Camera2D with its own viewportSize is in
-- play (see Camera2D.h).
function Player:Update(deltaTime, solids, worldWidth, worldHeight)
    self:HandleInput()
    self.body:Integrate(deltaTime)

    self.body:ResolveWindowBounds(worldWidth or eWindow:GetWidth(), worldHeight or eWindow:GetHeight())
    if solids then
        for _, solid in ipairs(solids) do
            self.body:ResolveCollisionWith(solid)
        end
    end

    -- Legs last: they follow wherever the torso actually ENDED UP this
    -- frame (post-collision), so a foot never plants at a position the
    -- body then gets pushed out of.
    self:UpdateLegs(deltaTime, solids)
end

-- Far leg, torso, near leg -- the shade multiplier on the back leg (see
-- LegRig.Defaults.legs) plus this ordering is what sells the depth
-- without a second set of art or any z-buffer.
function Player:Draw()
    self:DrawLegs("back")
    DrawBody(self.body)
    self:DrawLegs("front")
end

return Player