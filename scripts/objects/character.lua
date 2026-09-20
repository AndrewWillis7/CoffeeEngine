-- Shared base for every legged, rigid-body character in the scene --
-- the player and any NPC alike. Owns torso sizing, the collider, and
-- the generic physics/legs update+draw loop; HandleInput is left to
-- the subclass (real keyboard input for Player, a tiny AI for NPC),
-- the same split Player used to own entirely by itself before NPCs
-- existed.
--
-- Inherits LegRig (objects/leg_rig.lua), same as Player used to
-- directly -- a Character IS a legged actor.

local Class = require("core.Class")
local LegRig = require("objects.leg_rig")
local Torso = require("objects.torso")

local Character = Class(LegRig)

-- h is the TOTAL character height (torso + legs) -- see the ASCII
-- diagram this comment used to carry on Player.lua; unchanged by the
-- move here. legConfig is forwarded straight to LegRig. torsoConfig is
-- forwarded straight to Torso (objects/torso.lua) -- see its Defaults
-- for the undershirt/overshirt knobs (neckline, hem, coat length, ...).
function Character.new(x, y, w, h, legConfig, torsoConfig)
    w = w or 50
    h = h or 50

    -- Base part first, then re-tag -- the inheritance pattern documented
    -- in core/Class.lua. The rig has to exist before the torso can be
    -- sized, because GetStandHeight() is what decides how much of `h` is
    -- leg rather than torso.
    local self = LegRig.new(legConfig)
    setmetatable(self, Character)

    local standHeight = math.min(self:GetStandHeight(), h - 2)
    local torsoHeight = math.max(2, math.floor(h - standHeight + 0.5))

    -- Torso height forced EVEN -- see LegRig:BuildCanvases' even-size
    -- comment for why a fractional hip offset would crawl the hip seam.
    torsoHeight = torsoHeight - (torsoHeight % 2)
    if torsoHeight < 2 then torsoHeight = 2 end
    standHeight = h - torsoHeight

    -- Torso owns the actual RigidBody2D/sprite (generated, not a flat-
    -- color quad -- makes the character pixel-addressable, so lighting
    -- shows up on it) plus the undershirt/overshirt cloth layers.
    self.torso = Torso.new(x, y, w, torsoHeight, standHeight, torsoConfig)
    self.body = self.torso.body
    self.sprite = self.torso.sprite

    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2, 0, standHeight / 2))

    -- Every Character gets a PlayerActorConfig for its moveSpeed/
    -- jumpForce/inputEnabled fields -- it's just a tuning bag, useful for
    -- an NPC too, not something that requires being player-controlled.
    -- One thing this does mean: Actors.GetPlayer() (C++ side, see
    -- ActorRegistry::GetPlayerActor) resolves to the FIRST body in
    -- creation order that has one of these attached, so the real,
    -- controllable player must always be constructed before any NPC for
    -- that lookup to keep resolving correctly -- CharacterFactory always
    -- creates the player first for exactly this reason.
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

-- No-op by default -- Player reads real input, NPC runs its own AI;
-- both call self.body:SetVelocity() from their own override of this.
function Character:HandleInput(deltaTime)
end

-- solids: array of RigidBody2D-or-wrapper to resolve collisions against
-- this frame -- see Player.lua's old comment for the full contract,
-- unchanged by the move here.
function Character:Update(deltaTime, solids, worldWidth, worldHeight)
    self:HandleInput(deltaTime)
    self.body:Integrate(deltaTime)

    self.body:ResolveWindowBounds(worldWidth or eWindow:GetWidth(), worldHeight or eWindow:GetHeight())
    if solids then
        for _, solid in ipairs(solids) do
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

    -- Coat sway last of all: it reads the lean/phase the leg update
    -- above just settled for this frame.
    local ox, oy = self.body:GetPosition()
    self.torso:UpdateCloth(deltaTime, ox, oy, self:GetLeanOffset(), self:GetFacing(), self.blend, self.phase)
end

function Character:Draw()
    self:DrawLegs("back")
    self.torso:DrawOvershirt("back")

    local dx, dy = self:GetLeanOffset(), self:GetBobOffset()
    if dx ~= 0 or dy ~= 0 then
        local x, y = self.body:GetPosition()
        self.body:SetPosition(x + dx, y + dy)
        DrawBody(self.body)
        self.body:SetPosition(x, y)
    else
        DrawBody(self.body)
    end

    self.torso:DrawOvershirt("front")
    self:DrawLegs("front")
end

return Character
