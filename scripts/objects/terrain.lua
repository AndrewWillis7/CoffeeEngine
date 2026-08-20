-- Wraps a RigidBody2D + PixelSprite + TerrainChunk as a single placeable
-- game object, same pattern Campfire/Camera/Player already use for their
-- own body + config pairs.
--
-- A Terrain is just an actor. There is no "the floor" anywhere in the
-- engine -- place as many of these as you like, anywhere, at any size,
-- with any seed; each one generates and animates independently. That's
-- the whole reason terrain is a Lua class over a C++ config object rather
-- than something the engine sets up for you.
--
-- WHAT IT IS UNDERNEATH
--   self.body   -- the actor. Position is the chunk's CENTER, size its
--                  texel dimensions (SetSprite derives that from the
--                  sprite, so w/h here is the single source of truth).
--   self.sprite -- a blank transparent PixelSprite the chunk paints into.
--                  Because it's a normal PixelSprite, terrain is lit
--                  per-pixel by LightingSystem for free, and
--                  PunchCircle-able the day destruction lands.
--   self.chunk  -- the TerrainChunk: generation config + the heightmap
--                  and grass simulation.
--
-- COLLISION
-- Deliberately NO CollisionShape2D. An uneven surface isn't a box, so
-- terrain resolves collision against its own heightmap instead -- see
-- Terrain:ResolveAgainst below and TerrainChunk.h's header comment.

local Class = require("core.Class")

local Terrain = Class()

--- @param x number World X of the chunk's CENTER
--- @param y number World Y of the chunk's CENTER
--- @param w number Width in texels
--- @param h number Height in texels. Must be tall enough for the dirt AND
---   the grass above it -- the chunk needs surfaceAmplitude +
---   grassMaxHeight + 1 texels of headroom above its mean surface, and
---   will warn and clamp if `opts.surfaceOffset` doesn't leave that room.
--- @param opts table|nil Optional overrides, all matching TerrainChunk's
---   own field names (see scripts/api/coffee_api.lua for the full list).
function Terrain.new(x, y, w, h, opts)
    opts = opts or {}

    local self = setmetatable({}, Terrain)

    self.body = RigidBody2D.new(x, y, w, h)

    -- Fully transparent canvas -- Generate() writes every pixel, dirt and
    -- sky alike, so nothing here is left to chance. Alpha 0 is what makes
    -- the area above the surface genuinely empty rather than black:
    -- PixelSprite::IsSolid is alpha-based, so transparent sky is also
    -- invisible to lighting and (eventually) to destruction queries.
    self.sprite = Sprite.NewSolid(w, h, 0.0, 0.0, 0.0, 0.0)
    self.body:SetSprite(self.sprite)

    -- Immovable, same convention as StaticBody -- terrain never
    -- integrates and never gets pushed. TerrainSystem also uses mass <= 0
    -- to decide a body can't part grass, so this keeps the ground from
    -- flattening its own vegetation.
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

    -- Everything above has to be set BEFORE this -- Generate() bakes the
    -- config into a heightmap, a pixel fill and a blade list, and changing
    -- a field afterward does nothing until the next Generate() call.
    self.chunk:Generate(self.sprite)

    -- Tag the body last, so TerrainSystem can never see a half-configured
    -- chunk if this ever moves off the main thread.
    self.body:SetTerrain(self.chunk)

    -- Off by default, matching the old flat floor. Turn it on and the
    -- ground casts real hard shadows -- a campfire sitting on the surface
    -- then lights the grass and the top layer of dirt but not the rock
    -- below it, which looks great and costs an occlusion test per light
    -- ray (see RigidBody2D::lightBlocking).
    if opts.lightBlocking then self.body:SetLightBlocking(true) end

    return self
end

--- Resolves `body` against this chunk's heightmap. Named to match the
--- same method on StaticBody so a level can put both in one `solids`
--- list and let Player:Update call them the same way, without the player
--- needing to know that one of them is a box and the other is a surface.
--- @param body userdata RigidBody2D to push out of the ground
function Terrain:ResolveAgainst(body)
    self.chunk:ResolveBody(body, self.body)
end

--- World Y of the ground surface directly under `worldX`. The way to
--- place anything ON the terrain without hand-tuning a Y: an object of
--- height h sits centered at (terrain:SurfaceYAt(x) - h / 2).
--- @param worldX number
--- @return number worldY
function Terrain:SurfaceYAt(worldX)
    return self.chunk:SurfaceWorldY(worldX, self.body)
end

function Terrain:Draw()
    DrawBody(self.body)
end

return Terrain