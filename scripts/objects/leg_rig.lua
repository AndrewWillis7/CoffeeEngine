-- Procedural two-bone leg rig -- the shared "legs" component for the
-- player AND every NPC. Owns nothing but its own segment bodies; the
-- OWNER body (torso) is passed in via SetOwner and is never moved by
-- this class -- legs follow the torso, they don't drive it.
--
-- Each leg is three drawn modules along one chain:
--
--     hip  o
--          |  legging   (upper leg -- width/height/color)
--     knee =            (joint spacer -- width/height/color)
--           \  boot     (lower leg + foot -- width/height/color)
--            o  foot   <- ground-snapped
--
-- IK is closed-form two-bone (law of cosines): L1 = legging.height +
-- knee.height, L2 = boot.height. The knee spacer rides on the END of
-- the upper bone and is oriented along it, so raising knee.height
-- pushes the bend point further down the leg -- that's the "leg format"
-- knob (short-shin digitigrade vs. long-shin human vs. no knee at all
-- when knee.height = 0).

local Class = require("core.Class")

local LegRig = Class()

-- ---------------------------------------------------------------------
-- Tunables. Every one of these is overridable per-rig via the config
-- table passed to LegRig.new(), and the three modules are additionally
-- overridable PER LEG (see Defaults.legs below) so a limping NPC or an
-- asymmetric creature doesn't need a second rig class.
-- ---------------------------------------------------------------------
LegRig.Defaults = {
    legging = { width = 5, height = 9, color = {0.30, 0.33, 0.50} },
    knee    = { width = 5, height = 2, color = {0.20, 0.22, 0.34} },
    boot    = { width = 6, height = 5, color = {0.14, 0.12, 0.16} },

    -- Fraction of full leg length the hip rests at while standing. 1.0
    -- means the leg is dead straight when grounded on flat terrain --
    -- geometrically correct but visually stiff, and it leaves the IK
    -- solver permanently at its singular fully-extended pose, so the
    -- knee has no consistent side to pop toward. Anything below ~0.95
    -- keeps a permanent, stable bend.
    stand = 0.88,

    -- World distance travelled per FULL gait cycle (both legs step
    -- once). The per-foot sweep amplitude is derived from this
    -- (stride/4, see UpdateLegs) rather than configured separately,
    -- because that exact ratio is what makes a planted foot hold still
    -- in WORLD space while the body moves over it -- any other
    -- amplitude and the feet visibly ice-skate.
    stride = 20,

    stepHeight = 3,   -- peak lift of a swinging foot, texels
    idleSpeed  = 4,   -- |vx| below this counts as standing still
    gaitBlend  = 8,   -- how fast the walk cycle fades in/out

    -- How fast a foot eases across a DISCONTINUITY -- a step up onto a
    -- ledge, a facing flip, landing after a jump. Deliberately NOT a lag
    -- filter on the foot position itself: filtering the position makes a
    -- planted foot chase the body a fraction of a frame behind, which is
    -- exactly the ice-skating artifact the stride math above exists to
    -- avoid (measured at ~4 texels of slide per stance before this was
    -- split out). Continuous motion is tracked EXACTLY; only the leftover
    -- offset from a jump decays, and ground height gets its own smoothing
    -- so a step up eases in. See UpdateLegs.
    smoothing  = 24,

    -- How far BELOW full extension to keep searching for ground. Lets a
    -- foot reach down into a dip instead of dangling over it; beyond
    -- this the leg just hangs straight (walking off a ledge).
    snapDistance = 6,

    -- Airborne pose: fraction of full leg length the feet hang at while
    -- rising vs. falling. Tucked on the way up, reaching on the way down.
    airTuck  = 0.68,
    airReach = 0.97,

    -- hipX is in LOCAL texels and is mirrored by facing. phase offsets
    -- the leg in the gait cycle (0.5 = perfectly opposed). layer picks
    -- which side of the torso it draws on, shade is a flat multiplier
    -- that fakes depth on the far leg without a second set of art.
    legs = {
        { hipX = 0, phase = 0.0, layer = "front", shade = 1.00 },
        { hipX = 0, phase = 0.5, layer = "back",  shade = 0.62 },
    },
}

local PART_ORDER = { "legging", "knee", "boot" }

-- ---------------------------------------------------------------------
-- Local helpers
-- ---------------------------------------------------------------------

local function clamp(v, lo, hi)
    if v < lo then return lo elseif v > hi then return hi end
    return v
end

-- Framerate-independent exponential approach -- same shape as
-- Camera2D::Follow and RigidBody2D's drag, and for the same reason: a
-- plain `current + (target - current) * rate * dt` overshoots (and at
-- high rate/low framerate, oscillates) once rate * dt > 1.
local function approach(current, target, rate, dt)
    if rate <= 0 then return target end
    return current + (target - current) * (1.0 - math.exp(-rate * dt))
end

local function pick(a, b)
    if a == nil then return b end
    return a
end

-- Rounds to a whole texel -- every module is authored as a real
-- PixelSprite, and Sprite.NewSolid takes integers. Sizes that came out
-- of arithmetic (a designer scaling a creature by 1.3) have to land on
-- the grid before they get here.
local function texels(v, minimum)
    v = math.floor((v or 0) + 0.5)
    if v < minimum then v = minimum end
    return v
end

local function makeModule(src, def)
    src = src or {}
    local c = src.color or def.color
    return {
        width  = texels(pick(src.width,  def.width),  1),
        height = texels(pick(src.height, def.height), 0), -- 0 = module omitted
        color  = { c[1] or 1.0, c[2] or 1.0, c[3] or 1.0, c[4] or 1.0 },
    }
end

-- p in [0,1). First half is SWING (foot off the ground, travelling
-- forward), second half is STANCE (foot planted, sweeping backward
-- under the body). Returns sweep in [-1,1] and lift in [0,1].
local function gaitPose(p)
    if p < 0.5 then
        local t = p / 0.5
        return -1.0 + 2.0 * t, math.sin(math.pi * t)
    end
    local t = (p - 0.5) / 0.5
    return 1.0 - 2.0 * t, 0.0
end

-- Places a segment body so its LOCAL +Y axis runs from a -> b. The
-- shared vertex stage (scripts/shaders/quad.vert) rotates local corners
-- by the standard matrix in a y-down space, so local +Y maps to
-- (-sin, cos) -- hence atan(-dx, dy) rather than the usual atan(dy, dx).
-- Rotation crosses the Lua boundary in DEGREES (see
-- Lua_RigidBody2DSetRotation).
local function placeSegment(body, ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    local len = math.sqrt(dx * dx + dy * dy)
    if len < 1e-5 then dx, dy, len = 0.0, 1.0, 1.0 end
    body:SetPosition((ax + bx) * 0.5, (ay + by) * 0.5)
    body:SetRotation(math.deg(math.atan(-dx / len, dy / len)))
end

-- A level's `solids` list holds wrapper OBJECTS (Terrain, StaticBody),
-- not raw bodies. Everything geometric here works off the underlying
-- RigidBody2D, so unwrap once and keep the wrapper around separately for
-- the ground query, which DOES care which kind of ground it's looking at.
-- A raw RigidBody2D passes through untouched, so an older level script
-- that still builds its list out of `.body`s keeps working.
local function solidBody(s)
    if type(s) == "table" then return s.body end
    return s
end

-- ---------------------------------------------------------------------
-- Construction
-- ---------------------------------------------------------------------

-- config == false (or a config with an empty `legs` list) builds a valid
-- but legless rig: GetStandHeight() returns 0 and every per-frame method
-- is a no-op, so an actor can inherit from LegRig unconditionally and
-- still be spawned without legs.
function LegRig.new(config)
    local self = setmetatable({}, LegRig)
    self:InitLegRig(config)
    return self
end

-- Split out from new() so a subclass can build ITS part first and then
-- initialize the inherited leg part onto the same table -- see
-- objects/player.lua. Safe to call again to rebuild the rig from a new
-- config (the old segment bodies are dropped; ActorRegistry owns them
-- and reclaims them on the next hot-reload).
function LegRig:InitLegRig(config)
    if config == false then config = { legs = {} } end
    config = config or {}

    local D = LegRig.Defaults

    self.stand        = pick(config.stand, D.stand)
    self.stride       = pick(config.stride, D.stride)
    self.stepHeight   = pick(config.stepHeight, D.stepHeight)
    self.idleSpeed    = pick(config.idleSpeed, D.idleSpeed)
    self.gaitBlendRate= pick(config.gaitBlend, D.gaitBlend)
    self.smoothing    = pick(config.smoothing, D.smoothing)
    self.snapDistance = pick(config.snapDistance, D.snapDistance)
    self.airTuck      = pick(config.airTuck, D.airTuck)
    self.airReach     = pick(config.airReach, D.airReach)

    local baseModules = {
        legging = makeModule(config.legging, D.legging),
        knee    = makeModule(config.knee,    D.knee),
        boot    = makeModule(config.boot,    D.boot),
    }
    self.modules = baseModules

    self.phase = 0.0
    self.blend = 0.0
    self.facing = 1
    self.owner = nil
    self.hipLocalY = 0.0
    self.solids = nil

    self.legs = {}
    local legDefs = config.legs or D.legs
    for i, def in ipairs(legDefs) do
        local leg = {
            hipX  = def.hipX or 0,
            phase = def.phase or ((i - 1) / #legDefs),
            layer = def.layer or "front",
            shade = def.shade or 1.0,
            -- +1 bends the knee toward the facing direction (forward,
            -- like a human); -1 bends it backward (like a bird's ankle).
            bend  = def.bend or 1,
            modules = {
                legging = def.legging and makeModule(def.legging, baseModules.legging) or baseModules.legging,
                knee    = def.knee    and makeModule(def.knee,    baseModules.knee)    or baseModules.knee,
                boot    = def.boot    and makeModule(def.boot,    baseModules.boot)    or baseModules.boot,
            },
            footX = nil, footY = nil,
        }
        leg.length = leg.modules.legging.height + leg.modules.knee.height + leg.modules.boot.height
        self.legs[i] = leg
        self:BuildLegParts(i)
    end

    -- The rig's reach is the SHORTEST leg's -- that's the one that
    -- decides how high the hips can sit before something dangles.
    self.legLength = nil
    for _, leg in ipairs(self.legs) do
        if not self.legLength or leg.length < self.legLength then self.legLength = leg.length end
    end
    self.legLength = self.legLength or 0

    -- Whole texels: this is what an owner subtracts from its total
    -- height to size its torso, and a fractional value there means a
    -- fractional torso sprite, which Sprite.NewSolid can't author.
    self.standHeight = texels(self.legLength * self.stand, 0)
end

function LegRig:BuildLegParts(index)
    local leg = self.legs[index]
    leg.parts = {}
    for _, name in ipairs(PART_ORDER) do
        local m = leg.modules[name]
        if m.height > 0 then
            -- Each leg gets its OWN sprite even when two legs share
            -- identical module settings -- LightingSystem writes its
            -- per-pixel lit overlay into the PixelSprite itself, so a
            -- shared sprite would mean the second leg's lighting
            -- overwrites the first's every frame.
            local sprite = Sprite.NewSolid(m.width, m.height, m.color[1], m.color[2], m.color[3], m.color[4])
            local body = RigidBody2D.new(0, 0, m.width, m.height)
            body:SetSprite(sprite)
            body:SetMass(0)             -- never integrated; nothing here falls
            body:SetColor(leg.shade, leg.shade, leg.shade, 1.0)
            leg.parts[name] = { body = body, sprite = sprite }
        end
    end
end

-- ---------------------------------------------------------------------
-- Wiring
-- ---------------------------------------------------------------------

-- hipLocalY is the hip's offset from the OWNER BODY'S CENTER, in texels,
-- +y down -- normally the torso's bottom edge (torsoHeight / 2).
function LegRig:SetOwner(body, hipLocalY)
    self.owner = body
    self.hipLocalY = hipLocalY or 0.0
    for _, leg in ipairs(self.legs) do
        leg.footX, leg.footY = nil, nil -- re-snap on the next update
        leg.groundY, leg.lastMode, leg.lastFacing = nil, nil, nil
    end
end

-- Optional: cache the ground set so UpdateLegs(dt) can be called with no
-- second argument (handy for NPCs that already know their own level).
function LegRig:SetSolids(solids) self.solids = solids end

function LegRig:GetStandHeight() return self.standHeight or 0 end
function LegRig:GetLegLength()   return self.legLength or 0 end
function LegRig:HasLegs()        return #self.legs > 0 end

function LegRig:SetFacing(f)
    if f and f ~= 0 then self.facing = (f > 0) and 1 or -1 end
end
function LegRig:GetFacing() return self.facing end

function LegRig:GetFootPosition(index)
    local leg = self.legs[index]
    if not leg then return nil end
    return leg.footX, leg.footY
end

-- ---------------------------------------------------------------------
-- Ground query
-- ---------------------------------------------------------------------

-- Highest solid surface at world x, searched downward from fromY to
-- maxY, or nil. Rotation is ignored, matching CollisionShape2D's own
-- axis-aligned simplification -- a rotated platform is probed as its
-- unrotated box.
--
-- Bodies are probed by their GetSize() box first (cheap reject), then --
-- if a PixelSprite is attached -- refined per-pixel with IsSolid down
-- the column. That second pass is what makes this work against carved
-- terrain rather than only flat StaticBody boxes: punch a hole in the
-- ground with PunchCircle and the feet drop into it on the very next
-- frame, no extra bookkeeping.
function LegRig:SampleGround(x, fromY, maxY, solids)
    solids = solids or self.solids
    if not solids then return nil end

        local best = nil
    for _, s in ipairs(solids) do
        local body = solidBody(s)
        if body and body ~= self.owner then
            local sx, sy = body:GetPosition()
            local sw, sh = body:GetSize()
            local left, right = sx - sw * 0.5, sx + sw * 0.5
            local top, bottom = sy - sh * 0.5, sy + sh * 0.5

            -- The X range check matters more than it looks for heightmap
            -- ground: SurfaceWorldY deliberately CLAMPS an out-of-range X
            -- to the nearest column (convenient when placing props), so
            -- without this a foot walking off the end of a chunk would
            -- keep snapping to the chunk's edge height out over the void.
            if x >= left and x <= right and bottom >= fromY and top <= maxY then
                local surface

                if type(s) == "table" and s.SurfaceYAt then
                    -- Heightmap ground -- ask it directly rather than
                    -- probing its pixels. This returns the top of the
                    -- DIRT, which is what bodies stand on; the per-pixel
                    -- path below would plant the foot on a blade of grass
                    -- instead, because blades are solid pixels in the same
                    -- sprite and stand up to grassMaxHeight above the real
                    -- surface. It's also the exact same number
                    -- TerrainChunk::ResolveBody stands the collider on, so
                    -- the feet and the body agree by construction instead
                    -- of by coincidence.
                    surface = s:SurfaceYAt(x)
                else
                    surface = top
                    local sprite = body:GetSprite()
                    if sprite and sw > 0 and sh > 0 then
                        surface = nil
                        local tw, th = sprite:GetWidth(), sprite:GetHeight()
                        local col = math.floor((x - left) / sw * tw)
                        col = clamp(col, 0, tw - 1)
                        local startRow = math.floor((math.max(fromY, top) - top) / sh * th)
                        if startRow < 0 then startRow = 0 end
                        for row = startRow, th - 1 do
                            if sprite:IsSolid(col, row) then
                                surface = top + (row / th) * sh
                                break
                            end
                        end
                    end
                end

                if surface and surface >= fromY and surface <= maxY then
                    if not best or surface < best then best = surface end
                end
            end
        end
    end
    return best
end

-- ---------------------------------------------------------------------
-- Per-frame
-- ---------------------------------------------------------------------

-- Named UpdateLegs/DrawLegs rather than Update/Draw so a subclass
-- (Player) can define its own Update/Draw without shadowing these.
-- Call AFTER the owner's physics/collision has resolved for the frame --
-- the legs are reacting to where the torso ended up, not predicting it.
function LegRig:UpdateLegs(dt, solids)
    if not self.owner or #self.legs == 0 then return end
    solids = solids or self.solids

    local ox, oy = self.owner:GetPosition()
    local vx, vy = self.owner:GetVelocity()
    local grounded = self.owner:IsGrounded()
    local speed = math.abs(vx)

    self:SetFacing(speed > self.idleSpeed and vx or nil)

    -- Phase advances with DISTANCE, not time, so the gait automatically
    -- matches whatever speed the owner happens to be moving at -- a
    -- sprinting NPC and a shuffling one share this code and neither
    -- needs a hand-tuned animation rate.
    if grounded and speed > self.idleSpeed then
        self.phase = (self.phase + (speed * dt) / self.stride) % 1.0
        self.blend = approach(self.blend, 1.0, self.gaitBlendRate, dt)
    else
        self.blend = approach(self.blend, 0.0, self.gaitBlendRate, dt)
    end

    -- Airborne pose: tucked while rising, reaching while falling.
    local airFactor = self.airTuck
    if not grounded then
        local t = clamp(vy / 200.0, -1.0, 1.0)
        airFactor = self.airTuck + (self.airReach - self.airTuck) * ((t + 1.0) * 0.5)
    end

    local hipY = oy + self.hipLocalY

    for _, leg in ipairs(self.legs) do
        local hipX = ox + leg.hipX * self.facing

        local sweep, lift = gaitPose((self.phase + leg.phase) % 1.0)
        -- stride/4 is the zero-slide amplitude: over one stance half-
        -- cycle the body advances stride/2 while the foot sweeps
        -- stride/2 backward relative to the hip, netting out to a foot
        -- that holds still in world space. See Defaults.stride.
        local amp = self.stride * 0.25 * self.blend

        local targetX = hipX + sweep * amp * self.facing

        -- Ground height is the one thing that gets smoothed, and it's
        -- smoothed on its OWN (a step up eases in over a few frames)
        -- rather than by filtering the final foot position -- so the
        -- swing arc and the planted stance both stay exact.
        local mode = "air"
        local targetY
        if grounded then
            local surface = self:SampleGround(targetX, hipY, hipY + leg.length + self.snapDistance, solids)
            if surface then
                mode = "ground"
                leg.groundY = leg.groundY and approach(leg.groundY, surface, self.smoothing, dt) or surface
                targetY = leg.groundY - lift * self.stepHeight * self.blend
            else
                targetY = hipY + leg.length * self.airReach -- out over a ledge: hang
            end
        else
            targetY = hipY + leg.length * airFactor
        end
        if mode ~= "ground" then leg.groundY = nil end

        -- Never let a foot climb above the hip or past full extension.
        targetY = clamp(targetY, hipY + leg.length * 0.25, hipY + leg.length)

        if leg.footX == nil then
            leg.offX, leg.offY = 0.0, 0.0
        else
            -- Absorb the two discontinuities we can name -- a facing flip
            -- mirrors the whole sweep, and entering/leaving the ground
            -- swaps which pose drives the foot -- into a decaying offset,
            -- so the foot EASES across them instead of teleporting, while
            -- every other frame tracks its target exactly.
            if self.facing ~= leg.lastFacing or mode ~= leg.lastMode then
                leg.offX = leg.footX - targetX
                leg.offY = leg.footY - targetY
            end
            leg.offX = approach(leg.offX, 0.0, self.smoothing, dt)
            leg.offY = approach(leg.offY, 0.0, self.smoothing, dt)
        end

        leg.lastFacing, leg.lastMode = self.facing, mode
        leg.footX = targetX + leg.offX
        leg.footY = targetY + leg.offY

        self:SolveLeg(leg, hipX, hipY)
    end
end

-- Closed-form two-bone IK. L1 is the upper chain (legging + knee
-- spacer), L2 the boot. The knee lands on the circle intersection,
-- pushed to whichever side `bend` and the current facing select.
function LegRig:SolveLeg(leg, hipX, hipY)
    local L1 = leg.modules.legging.height + leg.modules.knee.height
    local L2 = leg.modules.boot.height

    local dx, dy = leg.footX - hipX, leg.footY - hipY
    local d = math.sqrt(dx * dx + dy * dy)
    if d < 1e-5 then dx, dy, d = 0.0, 1.0, 1.0 end

    -- Clamped just inside both singularities: exactly at full extension
    -- (or full fold) the knee's offset from the hip->foot line is zero
    -- and its side becomes numerically undecided, which reads as a
    -- one-frame knee flip.
    local dClamped = clamp(d, math.abs(L1 - L2) + 0.01, L1 + L2 - 0.01)

    local ux, uy = dx / d, dy / d
    local footX, footY = hipX + ux * dClamped, hipY + uy * dClamped

    -- Distance along hip->foot to the knee's projection, plus its
    -- perpendicular offset (law of cosines).
    local a = (L1 * L1 - L2 * L2 + dClamped * dClamped) / (2.0 * dClamped)
    local h = math.sqrt(math.max(0.0, L1 * L1 - a * a))

    -- (uy, -ux) is the perpendicular that points toward +x when the leg
    -- hangs straight down, so bend * facing puts the knee in front of
    -- the actor in whichever direction it's currently facing.
    local side = leg.bend * self.facing
    local nx, ny = uy * side, -ux * side

    local kneeX = hipX + ux * a + nx * h
    local kneeY = hipY + uy * a + ny * h

    -- Redistribute the chain along the two solved directions using each
    -- module's EXACT authored height, so no segment is ever stretched --
    -- the quads stay their native pixel size and only rotate.
    local upLen = math.max(L1, 1e-5)
    local u1x, u1y = (kneeX - hipX) / upLen, (kneeY - hipY) / upLen

    local lowLen = math.sqrt((footX - kneeX) ^ 2 + (footY - kneeY) ^ 2)
    if lowLen < 1e-5 then lowLen = 1.0 end
    local u2x, u2y = (footX - kneeX) / lowLen, (footY - kneeY) / lowLen

    local legging = leg.parts.legging
    local knee    = leg.parts.knee
    local boot    = leg.parts.boot

    local thighLen = leg.modules.legging.height
    local ax, ay = hipX + u1x * thighLen, hipY + u1y * thighLen
    if legging then placeSegment(legging.body, hipX, hipY, ax, ay) end
    if knee    then placeSegment(knee.body, ax, ay, kneeX, kneeY) end
    if boot    then placeSegment(boot.body, kneeX, kneeY,
                                 kneeX + u2x * leg.modules.boot.height,
                                 kneeY + u2y * leg.modules.boot.height) end

    leg.solvedFootX = kneeX + u2x * leg.modules.boot.height
    leg.solvedFootY = kneeY + u2y * leg.modules.boot.height
    leg.kneeX, leg.kneeY = kneeX, kneeY
end

-- layer: "back", "front", or nil for all of them. Split so an owner can
-- sandwich its torso between the two -- see Player:Draw().
function LegRig:DrawLegs(layer)
    for _, leg in ipairs(self.legs) do
        if layer == nil or leg.layer == layer then
            for _, name in ipairs(PART_ORDER) do
                local part = leg.parts[name]
                if part then DrawBody(part.body) end
            end
        end
    end
end

function LegRig:Draw()
    self:DrawLegs("back")
    self:DrawLegs("front")
end

return LegRig