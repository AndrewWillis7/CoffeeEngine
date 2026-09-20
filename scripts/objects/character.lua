-- Shared base for every legged character, player and NPC alike. Owns torso
-- sizing, the collider, and the generic physics/legs update and draw loop;
-- HandleInput is the subclass's job. Inherits LegRig -- a Character IS a
-- legged actor.

local Class = require("core.Class")
local LegRig = require("objects.leg_rig")
local Torso = require("objects.torso")

local Character = Class(LegRig)

-- h is the TOTAL height, torso plus legs. legConfig goes straight to LegRig and
-- torsoConfig straight to Torso -- see Torso's Defaults for the clothing knobs.
function Character.new(x, y, w, h, legConfig, torsoConfig)
    w = w or 50
    h = h or 50

    -- Base part first, then re-tag (see core/Class.lua). The rig must exist
    -- before the torso can be sized: GetStandHeight() decides how much of `h`
    -- is leg rather than torso.
    local self = LegRig.new(legConfig)
    setmetatable(self, Character)

    local standHeight = math.min(self:GetStandHeight(), h - 2)
    local torsoHeight = math.max(2, math.floor(h - standHeight + 0.5))

    -- Forced EVEN: a fractional hip offset makes the hip seam crawl.
    torsoHeight = torsoHeight - (torsoHeight % 2)
    if torsoHeight < 2 then torsoHeight = 2 end
    standHeight = h - torsoHeight

    -- Torso owns the body and its generated sprite -- pixel-addressable, so
    -- lighting shows up on it -- plus the cloth layers.
    self.torso = Torso.new(x, y, w, torsoHeight, standHeight, torsoConfig)
    self.body = self.torso.body
    self.sprite = self.torso.sprite

    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2, 0, standHeight / 2))

    -- Every Character gets one: it is a tuning bag (moveSpeed, jumpForce,
    -- inputEnabled), not a claim to being player-controlled. It does mean
    -- Actors.GetPlayer() resolves to the FIRST body created with one attached,
    -- which is why CharacterFactory always builds the player before any NPC.
    self.config = PlayerActorConfig.new()
    self.config:SetMoveSpeed(25)
    self.config:SetJumpForce(350)
    self.body:SetPlayerConfig(self.config)

    -- Hips at the torso's bottom edge, so feet land exactly on the collider's
    -- bottom edge -- which is what rests on the floor.
    self:SetOwner(self.body, torsoHeight / 2)

    self.torsoHeight = torsoHeight
    self.height = h

    return self
end

-- No-op by default. Player reads input, NPC runs its AI; both override this
-- and call self.body:SetVelocity() from it.
function Character:HandleInput(deltaTime)
end

-- solids is an array of RigidBody2D or wrapper tables to resolve against.
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

    -- Legs last, so they follow where the torso actually ENDED UP after
    -- collision and a foot never plants somewhere the body is then pushed out of.
    self:UpdateLegs(deltaTime, solids)

    -- Coat sway last: it reads the lean and phase the leg update just settled.
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
