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

    local standHeight = math.min(self:GetStandHeight(), h - 2)
    local torsoHeight = math.max(2, math.floor(h - standHeight + 0.5))

    -- Torso height is forced EVEN, which is not cosmetic. hipLocalY below
    -- is torsoHeight / 2, and LegRig anchors its leg canvas to the owner's
    -- center by that offset; an odd torso puts the hip -- and therefore
    -- the canvas's whole pixel grid -- half a texel off the torso's own,
    -- so the two would round to different texels under quad.vert's
    -- u_PixelSnap and the hip seam would crawl as the player walks. See
    -- LegRig:BuildCanvases' even-size comment for the other half of this.
    torsoHeight = torsoHeight - (torsoHeight % 2)
    if torsoHeight < 2 then torsoHeight = 2 end
    standHeight = h - torsoHeight

    self.body = RigidBody2D.new(x, y, w, torsoHeight)

    -- Solid white generated sprite instead of a flat-color quad -- makes
    -- the player pixel-addressable too, so lighting (a torch flickering
    -- across the player as they walk by) actually shows up on them.
    self.sprite = Sprite.NewSolid(w, torsoHeight, 1.0, 1.0, 1.0, 1.0)
    self.body:SetSprite(self.sprite)

    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2, 0, standHeight / 2))

    self.config = PlayerActorConfig.new()
    self.config:SetMoveSpeed(25)
    self.config:SetJumpForce(350)
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
            -- A level's `solids` list holds wrapper OBJECTS (Terrain,
            -- StaticBody), not raw bodies, and each one knows how to push
            -- something out of itself -- box-vs-box for StaticBody, a
            -- heightmap sweep for Terrain. Dispatching through
            -- ResolveAgainst is what lets one list hold both without the
            -- player knowing which kind of ground it's standing on. A raw
            -- RigidBody2D still works (it's what the list used to hold),
            -- so an older level script doesn't break.
            if type(solid) == "table" then
                if solid.ResolveAgainst then solid:ResolveAgainst(self.body) end
            else
                self.body:ResolveCollisionWith(solid)
            end
        end
    end

    -- Legs last: they follow wherever the torso actually ENDED UP this
    -- frame (post-collision), so a foot never plants at a position the
    -- body then gets pushed out of.
    self:UpdateLegs(deltaTime, solids)
end

function Player:Draw()
    self:DrawLegs("back")

    local bob = self:GetBobOffset()
    if bob ~= 0 then
        local x, y = self.body:GetPosition()
        self.body:SetPosition(x, y + bob)
        DrawBody(self.body)
        self.body:SetPosition(x, y)
    else
        DrawBody(self.body)
    end

    self:DrawLegs("front")
end

return Player