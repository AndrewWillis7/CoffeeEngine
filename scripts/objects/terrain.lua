-- A RigidBody2D plus a PixelSprite plus a TerrainChunk, as one placeable actor.
-- There is no "the floor" in this engine: place as many of these as you like,
-- any size, any seed, each generating and animating independently. That is why
-- terrain is a Lua class over a C++ config object rather than engine furniture.
--
--   self.body   -- the actor; position is the chunk's CENTRE, size its texels.
--   self.sprite -- a blank transparent canvas the chunk paints into. Being an
--                  ordinary PixelSprite, it is lit per-pixel for free and
--                  PunchCircle-able the day destruction lands.
--   self.chunk  -- generation config, heightmap and grass simulation.
--
-- Deliberately NO CollisionShape2D: an uneven surface isn't a box, so terrain
-- resolves against its own heightmap instead.

local Class = require("core.Class")

local Terrain = Class()

--- @param x number World X of the chunk's CENTER
--- @param y number World Y of the chunk's CENTER
--- @param w number Width in texels
--- @param h number Height in texels. Must fit the dirt AND the grass above it:
---   the chunk needs surfaceAmplitude + grassMaxHeight + 1 texels of headroom
---   over its mean surface, and warns and clamps if surfaceOffset doesn't.
--- @param opts table|nil Overrides matching TerrainChunk's own field names.
function Terrain.new(x, y, w, h, opts)
    opts = opts or {}

    local self = setmetatable({}, Terrain)

    self.body = RigidBody2D.new(x, y, w, h)

    -- Generate() writes every pixel, dirt and sky alike. Alpha 0 is what makes
    -- the area above the surface genuinely empty rather than black -- IsSolid is
    -- alpha-based, so transparent sky is invisible to lighting too.
    self.sprite = Sprite.NewSolid(w, h, 0.0, 0.0, 0.0, 0.0)
    self.body:SetSprite(self.sprite)

    -- Immovable, same convention as StaticBody. TerrainSystem also reads
    -- mass <= 0 as "can't part grass", so the ground won't flatten its own.
    self.body:SetMass(0)

    self.chunk = TerrainChunk.new()

    -- Surface
    if opts.seed then self.chunk:SetSeed(opts.seed) end
    if opts.surfaceFrequency then self.chunk:SetSurfaceFrequency(opts.surfaceFrequency) end
    if opts.surfaceAmplitude then self.chunk:SetSurfaceAmplitude(opts.surfaceAmplitude) end
    if opts.surfaceOctaves then self.chunk:SetSurfaceOctaves(opts.surfaceOctaves) end
    if opts.surfaceOffset then self.chunk:SetSurfaceOffset(opts.surfaceOffset) end

    -- Dirt
    if opts.dirtDark then self.chunk:SetDirtDark(table.unpack(opts.dirtDark)) end
    if opts.dirtLight then self.chunk:SetDirtLight(table.unpack(opts.dirtLight)) end
    if opts.rockColor then self.chunk:SetRockColor(table.unpack(opts.rockColor)) end
    if opts.topsoilColor then self.chunk:SetTopsoilColor(table.unpack(opts.topsoilColor)) end
    if opts.dirtToneSteps then self.chunk:SetDirtToneSteps(opts.dirtToneSteps) end
    if opts.rockChance then self.chunk:SetRockChance(opts.rockChance) end

    -- Grass
    if opts.grassDark then self.chunk:SetGrassDark(table.unpack(opts.grassDark)) end
    if opts.grassLight then self.chunk:SetGrassLight(table.unpack(opts.grassLight)) end
    if opts.grassMinHeight then self.chunk:SetGrassMinHeight(opts.grassMinHeight) end
    if opts.grassMaxHeight then self.chunk:SetGrassMaxHeight(opts.grassMaxHeight) end
    if opts.grassDensity then self.chunk:SetGrassDensity(opts.grassDensity) end
    if opts.swayAmplitude then self.chunk:SetSwayAmplitude(opts.swayAmplitude) end
    if opts.swaySpeed then self.chunk:SetSwaySpeed(opts.swaySpeed) end
    if opts.disturbStrength then self.chunk:SetDisturbStrength(opts.disturbStrength) end

    -- Collision
    if opts.maxStepHeight then self.chunk:SetMaxStepHeight(opts.maxStepHeight) end

    -- Everything above must be set BEFORE this: Generate() bakes the config into
    -- a heightmap, a fill and a blade list, and later changes do nothing until
    -- the next Generate().
    self.chunk:Generate(self.sprite)

    -- Tagged last, so TerrainSystem can never see a half-configured chunk.
    self.body:SetTerrain(self.chunk)

    -- Off by default. On, the ground casts hard shadows: a campfire on the
    -- surface lights the grass and topsoil but not the rock below, at the cost
    -- of an occlusion test per light ray.
    if opts.lightBlocking then self.body:SetLightBlocking(true) end

    return self
end

--- Resolves `body` against this chunk's heightmap. Named to match StaticBody's
--- so a level can keep both in one `solids` list and call them identically,
--- without the mover knowing which is a box and which a surface.
--- @param body userdata RigidBody2D to push out of the ground
function Terrain:ResolveAgainst(body)
    self.chunk:ResolveBody(body, self.body)
end

--- World Y of the surface under `worldX`. The way to place anything on terrain
--- without hand-tuning: an object of height h centres at SurfaceYAt(x) - h / 2.
--- @param worldX number
--- @return number worldY
function Terrain:SurfaceYAt(worldX)
    return self.chunk:SurfaceWorldY(worldX, self.body)
end

function Terrain:Draw()
    DrawBody(self.body)
end

return Terrain