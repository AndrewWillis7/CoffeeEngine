-- Procedural two-bone leg rig -- the shared "legs" component for the
-- player AND every NPC. Owns nothing but its own leg CANVAS; the OWNER
-- body (torso) is passed in via SetOwner and is never moved by this
-- class -- legs follow the torso, they don't drive it.
--
-- Each leg is four drawn modules along one chain:
--
--     hip  o
--          |  legging   (upper leg -- width/height/color)
--     knee []           (joint cap -- width/height/color, ALWAYS
--           \            axis-aligned, never rotated)
--            \  boot    (lower leg -- width/height/color)
--     ankle   o
--            [==]  foot (axis-aligned block, points the way we face)
--
-- IK is closed-form two-bone (law of cosines): L1 = legging.height +
-- knee.height, L2 = boot.height, solving hip -> ANKLE. The foot hangs
-- below the ankle as a rigid block, so `leg.length` (hip -> sole, what
-- decides stand height) is L1 + L2 + foot.height. The knee cap rides on
-- the END of the upper bone, so raising knee.height pushes the bend
-- point further down the leg -- that's the "leg format" knob
-- (short-shin digitigrade vs. long-shin human vs. no cap at all when
-- knee.height = 0).
--
-- =====================================================================
-- ON-GRID RENDERING -- why there are no rotated quads here anymore
-- =====================================================================
-- The previous version gave every module its own RigidBody2D and set
-- transform.rotation to aim it down the bone. That is geometrically
-- correct and looks wrong: a rotated quad's edges land wherever the
-- angle puts them, at whatever subpixel offset, so a 4-texel-wide thigh
-- rendered at 17 degrees is a smear whose silhouette changes shape every
-- frame the angle drifts, and the joints leave visible pinholes where
-- two differently-rotated quads meet. On a 320x180 stage where one texel
-- is a deliberate, visible unit, that reads as mush.
--
-- Instead the whole rig rasterizes itself into ONE axis-aligned
-- PixelSprite canvas per draw layer, every frame the pose changes, via
-- PixelSprite::DrawLimb/FillRect (see their comments in PixelSprite.h).
-- Consequences, all of them good:
--
--   * Every leg pixel IS a texel. Nothing is ever sampled at an angle,
--     so a held pose is bit-identical frame to frame -- no shimmer.
--   * Joints are integers. The hip, knee, ankle and sole are rounded to
--     whole texels RELATIVE TO THE HIP before anything is drawn, so the
--     staircase a diagonal bone makes is stable instead of crawling.
--   * The foot never rotates. It's an axis-aligned block, which is what
--     every hand-drawn pixel-art character does and is most of why this
--     reads as a character rather than a linkage.
--   * Six rotated bodies collapse into (at most) two unrotated ones.
--     That's 6 draw calls -> 2, and it takes LightingSystem off its
--     per-pixel Rotated() path (see the "unrotated fast path" comments
--     in WorldToPixel/PixelToWorld) for every lit leg pixel.
--   * The canvas is only re-rasterized when the INTEGER pose actually
--     changes (see the dirty check in UpdateLegs). Standing still costs
--     nothing at all; walking skips a good share of frames for free,
--     because quantizing to texels is exactly what makes consecutive
--     frames identical. Measured at ~30% of canvas-frames skipped while
--     walking without pause, 100% while standing.
--
-- TRADEOFF, stated plainly: joints are snapped relative to the HIP, not
-- to absolute world coordinates. That keeps the legs welded to the torso
-- under quad.vert's u_PixelSnap (both are offset from the same body
-- position, so they round together and the hip seam can never split),
-- at the cost of a planted foot occasionally shifting one texel as the
-- body crosses a texel boundary. The other way round -- snapping feet to
-- the world grid -- makes the LEGS slip a texel against the TORSO
-- instead, which is far more visible. Feet win.

local Class = require("core.Class")

local LegRig = Class()

-- ---------------------------------------------------------------------
-- Tunables. Every one of these is overridable per-rig via the config
-- table passed to LegRig.new(), and the four modules are additionally
-- overridable PER LEG (see Defaults.legs below) so a limping NPC or an
-- asymmetric creature doesn't need a second rig class.
-- ---------------------------------------------------------------------
LegRig.Defaults = {
    legging = { width = 5, height = 9, color = {0.30, 0.33, 0.50} },
    knee    = { width = 5, height = 2, color = {0.20, 0.22, 0.34} },
    boot    = { width = 6, height = 5, color = {0.14, 0.12, 0.16} },

    -- The foot. Height 0 removes it entirely (the boot then ends at the
    -- ground, like the old rig did). Its color defaults to the boot's, so
    -- an existing config that never heard of feet still gets a sensible
    -- one instead of a mismatched block.
    foot    = { width = 7, height = 2 },

    -- Fraction of full leg length the hip rests at while standing. 1.0
    -- means the leg is dead straight when grounded on flat terrain --
    -- geometrically correct but visually stiff, and it leaves the IK
    -- solver permanently at its singular fully-extended pose, so the
    -- knee has no consistent side to pop toward. Anything below ~0.95
    -- keeps a permanent, stable bend.
    stand = 0.88,

    -- World distance travelled per FULL gait cycle (both legs step
    -- once). The per-foot sweep amplitude is DERIVED from this and
    -- stanceRatio (see UpdateLegs) rather than configured separately,
    -- because that exact ratio is what makes a planted foot hold still
    -- in WORLD space while the body moves over it -- any other amplitude
    -- and the feet visibly ice-skate.
    stride = 20,

    -- Fraction of the cycle each foot spends PLANTED. 0.5 is the
    -- degenerate case: one foot lifts the exact instant the other lands,
    -- so the character is never on two feet and the walk reads as a
    -- rocking limp. Real gaits overlap; anything above 0.5 buys a
    -- double-support window where both feet are down, which is what
    -- makes a walk look planted rather than tippy. Feeds the sweep
    -- amplitude, so raising it lengthens each step to match -- see
    -- UpdateLegs.
    stanceRatio = 0.58,

    -- Number of discrete poses the SWINGING (airborne) foot is allowed
    -- to occupy on its way forward. This is the "hand-animated" knob:
    -- a continuously interpolated swing slides the foot through every
    -- subpixel position between the two steps, which on a chunky pixel
    -- grid reads as a foot melting forward. Snapping it to a handful of
    -- poses gives the crisp pop between keyframes that drawn sprite
    -- animation has. Only the SWING is quantized -- stance stays exactly
    -- linear, because that's the half of the cycle the no-slide math
    -- above depends on. 0 disables it (fully continuous swing).
    swingFrames = 4,

    stepHeight = 3,   -- peak lift of a swinging foot, texels
    idleSpeed  = 4,   -- |vx| below this counts as standing still
    gaitBlend  = 8,   -- how fast the walk cycle fades in/out

    -- Peak rise of the HIPS at mid-stance, in whole texels. Driven off
    -- the swinging foot's own lift rather than a second hand-tuned
    -- phase offset -- one foot is at the top of its arc exactly when the
    -- other is straight underneath the body, which is exactly when a
    -- real walk is at its tallest, so the two are the same number. Whole
    -- texels only (it is rounded): a fractional bob would put the torso
    -- on a different subpixel offset from the legs every frame, which is
    -- the shimmer this whole file exists to avoid. 0 disables it.
    bob = 1,

    -- How far forward of the ankle the foot block sits, texels. 0
    -- centers it (a boot-shaped stump); 1-2 gives a toe that reads as a
    -- direction the character is facing even when it's standing still.
    footLean = 1,

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
    -- which canvas -- and therefore which side of the torso -- it draws
    -- on, shade is a flat multiplier BAKED INTO THE RASTERIZED PIXELS
    -- (not the body tint, which would apply to a whole canvas at once)
    -- that fakes depth on the far leg without a second set of art.
    --
    -- shade multiplies toward BLACK, so how far it can be pushed depends
    -- on how dark the modules already are: on an unlit night stage a
    -- 0.62 shade over an already-dark boot took the far leg below the
    -- background and it stopped reading as a leg at all. 0.70 is enough
    -- separation to sell the depth while keeping the far leg visible.
    --
    -- The two stock legs are offset one texel each way rather than
    -- sharing a hip. With both at hipX = 0 the sweep is the ONLY thing
    -- separating them, so the moment the character stops walking the two
    -- legs converge to the identical solved pose and the silhouette
    -- collapses to a single leg -- the idle pose was the one place the
    -- rig looked broken. Two texels of permanent stagger costs nothing
    -- while walking (each leg just plants a texel off center) and gives
    -- a standing character a stance.
    legs = {
        { hipX =  1, phase = 0.0, layer = "front", shade = 1.00 },
        { hipX = -1, phase = 0.5, layer = "back",  shade = 0.70 },
    },
}

-- Back first: the owner draws its torso between the two (see
-- Player:Draw), so this is literally far-side-to-near-side.
local LAYER_ORDER = { "back", "front" }

-- ---------------------------------------------------------------------
-- Local helpers
-- ---------------------------------------------------------------------

local floor, sqrt, abs, exp = math.floor, math.sqrt, math.abs, math.exp
local sin, pi, max = math.sin, math.pi, math.max

local function clamp(v, lo, hi)
    if v < lo then return lo elseif v > hi then return hi end
    return v
end

-- Round-half-up to a whole texel. Deliberately the SAME rule
-- quad.vert's u_PixelSnap uses (floor(x + 0.5)) so a position computed
-- here and a position snapped by the vertex stage never disagree about
-- which texel they mean.
local function round(v)
    return floor(v + 0.5)
end

-- Framerate-independent exponential approach -- same shape as
-- Camera2D::Follow and RigidBody2D's drag, and for the same reason: a
-- plain `current + (target - current) * rate * dt` overshoots (and at
-- high rate/low framerate, oscillates) once rate * dt > 1.
local function approach(current, target, rate, dt)
    if rate <= 0 then return target end
    return current + (target - current) * (1.0 - exp(-rate * dt))
end

local function pick(a, b)
    if a == nil then return b end
    return a
end

-- Rounds to a whole texel -- every module is rasterized as whole texels,
-- so sizes that came out of arithmetic (a designer scaling a creature by
-- 1.3) have to land on the grid before they get here.
local function texels(v, minimum)
    v = floor((v or 0) + 0.5)
    if v < minimum then v = minimum end
    return v
end

local function makeModule(src, def, fallbackColor)
    src = src or {}
    local c = src.color or def.color or fallbackColor or {1, 1, 1, 1}
    return {
        width  = texels(pick(src.width,  def.width),  1),
        height = texels(pick(src.height, def.height), 0), -- 0 = module omitted
        color  = { c[1] or 1.0, c[2] or 1.0, c[3] or 1.0, c[4] or 1.0 },
    }
end

-- p in [0,1). The first (1 - stanceRatio) of the cycle is SWING (foot
-- off the ground, travelling forward), the rest is STANCE (foot planted,
-- sweeping backward under the body). Returns sweep in [-1,1] and lift in
-- [0,1].
--
-- STANCE IS EXACTLY LINEAR and must stay that way -- it's the half of
-- the cycle where the foot is touching the world, and any easing there
-- turns into visible sliding. All the shaping lives in the swing, where
-- the foot is in the air and nothing can betray it:
--   * sweep is smoothstepped, so the foot leaves the ground and arrives
--     at its next plant gently instead of at a constant conveyor speed.
--   * lift peaks at ~40% rather than dead center, so the foot snaps up
--     off the ground and floats down onto it -- a flat landing reads as
--     weight, a symmetric arc reads as a pendulum.
local function gaitPose(p, stanceRatio, swingFrames)
    local swingSpan = 1.0 - stanceRatio
    if swingSpan <= 0.0 then return 1.0 - 2.0 * p, 0.0 end

    if p < swingSpan then
        local t = p / swingSpan
        if swingFrames and swingFrames > 0 then
            -- Quantize to discrete keyframes -- see Defaults.swingFrames.
            t = floor(t * swingFrames + 0.5) / swingFrames
        end
        local eased = t * t * (3.0 - 2.0 * t)
        return -1.0 + 2.0 * eased, sin(pi * (t ^ 0.8))
    end

    local t = (p - swingSpan) / stanceRatio
    return 1.0 - 2.0 * t, 0.0
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
-- config (the old canvas bodies/sprites are dropped; ActorRegistry owns
-- them and reclaims them on the next hot-reload).
function LegRig:InitLegRig(config)
    if config == false then config = { legs = {} } end
    config = config or {}

    local D = LegRig.Defaults

    self.stand        = pick(config.stand, D.stand)
    self.stride       = pick(config.stride, D.stride)
    self.stanceRatio  = clamp(pick(config.stanceRatio, D.stanceRatio), 0.05, 0.95)
    self.swingFrames  = pick(config.swingFrames, D.swingFrames)
    self.stepHeight   = pick(config.stepHeight, D.stepHeight)
    self.idleSpeed    = pick(config.idleSpeed, D.idleSpeed)
    self.gaitBlendRate= pick(config.gaitBlend, D.gaitBlend)
    self.smoothing    = pick(config.smoothing, D.smoothing)
    self.snapDistance = pick(config.snapDistance, D.snapDistance)
    self.airTuck      = pick(config.airTuck, D.airTuck)
    self.airReach     = pick(config.airReach, D.airReach)
    self.bob          = texels(pick(config.bob, D.bob), 0)
    self.footLean     = texels(pick(config.footLean, D.footLean), 0)

    local baseModules = {
        legging = makeModule(config.legging, D.legging),
        knee    = makeModule(config.knee,    D.knee),
        boot    = makeModule(config.boot,    D.boot),
    }
    -- Feet inherit the boot's color by default, so a pre-foot config
    -- (main.lua's player, every existing NPC) grows a matching one.
    baseModules.foot = makeModule(config.foot, D.foot, baseModules.boot.color)
    self.modules = baseModules

    self.phase = 0.0
    self.blend = 0.0
    self.facing = 1
    self.bobY = 0
    self.owner = nil
    self.hipLocalY = 0.0
    self.solids = nil

    self.legs = {}
    local legDefs = config.legs or D.legs
    for i, def in ipairs(legDefs) do
        local m = {
            legging = def.legging and makeModule(def.legging, baseModules.legging) or baseModules.legging,
            knee    = def.knee    and makeModule(def.knee,    baseModules.knee)    or baseModules.knee,
            boot    = def.boot    and makeModule(def.boot,    baseModules.boot)    or baseModules.boot,
        }
        m.foot = def.foot and makeModule(def.foot, baseModules.foot, m.boot.color) or baseModules.foot

        local leg = {
            hipX  = texels(def.hipX or 0, -1e9),
            phase = def.phase or ((i - 1) / #legDefs),
            layer = def.layer or "front",
            shade = def.shade or 1.0,
            -- +1 bends the knee toward the facing direction (forward,
            -- like a human); -1 bends it backward (like a bird's ankle).
            bend  = def.bend or 1,
            modules = m,
            footX = nil, footY = nil,
        }

        -- L1/L2 are the IK bones (hip -> knee -> ANKLE). `length` is the
        -- full hip -> SOLE reach, which is what stand height and the
        -- ground clamp care about, and includes the rigid foot block.
        leg.L1 = m.legging.height + m.knee.height
        leg.L2 = m.boot.height
        leg.chain = leg.L1 + leg.L2
        leg.length = leg.chain + m.foot.height

        self.legs[i] = leg
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

    self:BuildCanvases()
end

-- ---------------------------------------------------------------------
-- Canvas allocation
-- ---------------------------------------------------------------------

-- How far sideways off the hip->ankle line the knee can ever swing, in
-- texels. The knee is the apex of a triangle with sides L1, L2 and base
-- d, so its offset is 2 * area / d -- a smooth function of d with an
-- interior maximum, which means the closed-form bound (L1 * L2 /
-- |L1 - L2|, the right-angled case at the shortest possible base) is
-- correct but loose: for the stock player it overestimates by more than
-- half. Since d is confined to a known interval -- the target clamp in
-- UpdateLegs never lets a sole rise above quarter extension, and the IK
-- can't reach past L1 + L2 -- just SAMPLE the function across that
-- interval and take the real peak. This runs once, at rig construction,
-- and every texel it saves is a texel the lighting pass doesn't walk on
-- every light, every frame.
local function KneeBulge(leg)
    local L1, L2 = leg.L1, leg.L2
    if L1 <= 0 or L2 <= 0 then return 0.0 end

    local dMin = max(abs(L1 - L2), leg.length * 0.25 - leg.modules.foot.height)
    local dMax = L1 + L2
    if dMax <= dMin then return 0.0 end

    local peak = 0.0
    local samples = 64
    for i = 0, samples do
        local d = dMin + (dMax - dMin) * (i / samples)
        if d > 1e-5 then
            local a = (L1 * L1 - L2 * L2 + d * d) / (2.0 * d)
            local hh = L1 * L1 - a * a
            if hh > 0.0 then
                local bulge = sqrt(hh)
                if bulge > peak then peak = bulge end
            end
        end
    end
    return peak
end

-- One PixelSprite per draw layer that actually has legs on it, sized
-- once here to the worst-case footprint the solver can produce, and
-- reused forever after (nothing below ever reallocates).
--
-- WIDTH is the interesting one. A leg reaches sideways by three
-- independent amounts that all stack: the gait's own sweep, the knee's
-- sideways bulge when the leg folds, and half the widest module. The
-- bulge is measured rather than bounded -- see KneeBulge above.
--
-- BOTH DIMENSIONS ARE FORCED EVEN, and that is load-bearing rather than
-- tidiness: the canvas body is centered on the owner's position, so its
-- left/top edge sits at (owner center - size/2). An odd size puts that
-- edge on a half-texel, which offsets the canvas's whole pixel grid half
-- a texel from the torso's -- so the two would sample against different
-- grids and the hip seam would crawl. Even sizes keep both on the same
-- one. (Same reason SetOwner rounds hipLocalY.)
function LegRig:BuildCanvases()
    self.canvases = {}
    if #self.legs == 0 then
        self.canvasW, self.canvasH, self.baseHipRow, self.canvasOffY = 0, 0, 0, 0
        return
    end

    local maxAmp = self.stride * self.stanceRatio * 0.5
    local halfW, maxLen = 0, 0

    for _, leg in ipairs(self.legs) do
        local m = leg.modules
        local widest = max(m.legging.width, m.knee.width, m.boot.width, m.foot.width)
        local reach = abs(leg.hipX) + maxAmp + KneeBulge(leg) + widest * 0.5 + self.footLean + 1
        if reach > halfW then halfW = reach end
        if leg.length > maxLen then maxLen = leg.length end
    end

    local w = math.ceil(halfW) * 2
    if w % 2 == 1 then w = w + 1 end

    -- baseHipRow leaves room ABOVE the hip for the bob to raise it into;
    -- rows below run to hip + full extension, which the target clamp in
    -- UpdateLegs guarantees is the deepest a sole can go. +2 of slack
    -- absorbs the foot block and rounding.
    self.baseHipRow = 1 + self.bob
    local h = self.baseHipRow + maxLen + 2
    if h % 2 == 1 then h = h + 1 end

    self.canvasW, self.canvasH = w, h

    local used = {}
    for _, leg in ipairs(self.legs) do used[leg.layer] = true end

    for _, layer in ipairs(LAYER_ORDER) do
        if used[layer] then
            -- Fully transparent to start -- the canvas is a scratch
            -- surface, not art; every visible pixel on it is written by
            -- RasterizeLeg below.
            local sprite = Sprite.NewSolid(w, h, 0, 0, 0, 0)
            local body = RigidBody2D.new(0, 0, w, h)
            body:SetSprite(sprite)
            body:SetMass(0)          -- never integrated; nothing here falls
            -- White tint: per-leg shade is baked into the pixels instead
            -- (see Defaults.legs), because one canvas can carry several
            -- legs and a body tint can't tell them apart.
            body:SetColor(1.0, 1.0, 1.0, 1.0)
            self.canvases[layer] = { sprite = sprite, body = body, dirty = true }
        end
    end
end

-- ---------------------------------------------------------------------
-- Wiring
-- ---------------------------------------------------------------------

-- hipLocalY is the hip's offset from the OWNER BODY'S CENTER, in texels,
-- +y down -- normally the torso's bottom edge (torsoHeight / 2). Rounded
-- to a whole texel: it's what anchors the canvas's pixel grid to the
-- owner's, and half a texel of offset there is exactly the hip-seam
-- crawl BuildCanvases' even-size rule exists to prevent. An owner with
-- an odd-height torso should round its own height instead of relying on
-- this (see Player.new).
function LegRig:SetOwner(body, hipLocalY)
    self.owner = body
    self.hipLocalY = round(hipLocalY or 0.0)

    -- Distance from the owner's CENTER to the canvas's center. Integer by
    -- construction (integer hipLocalY, integer baseHipRow, even canvasH),
    -- which is what keeps torso and legs rounding to the same grid.
    self.canvasOffY = self.hipLocalY - self.baseHipRow + self.canvasH * 0.5

    for _, leg in ipairs(self.legs) do
        leg.footX, leg.footY = nil, nil -- re-snap on the next update
        leg.groundY, leg.lastMode, leg.lastFacing = nil, nil, nil
    end
    self:MarkPoseDirty()
end

-- Optional: cache the ground set so UpdateLegs(dt) can be called with no
-- second argument (handy for NPCs that already know their own level).
function LegRig:SetSolids(solids) self.solids = solids end

function LegRig:GetStandHeight() return self.standHeight or 0 end
function LegRig:GetLegLength()   return self.legLength or 0 end
function LegRig:HasLegs()        return #self.legs > 0 end

-- Whole-texel vertical offset the OWNER should draw its torso at this
-- frame, so the body rides the walk instead of gliding along at a fixed
-- height while the legs work underneath it. Negative is up. See
-- Defaults.bob and Player:Draw.
function LegRig:GetBobOffset() return self.bobY or 0 end

function LegRig:SetFacing(f)
    if f and f ~= 0 then self.facing = (f > 0) and 1 or -1 end
end
function LegRig:GetFacing() return self.facing end

-- World-space SOLE position (where the foot meets the ground), not the
-- ankle -- that's the point gameplay cares about (footstep particles,
-- dust, sound).
function LegRig:GetFootPosition(index)
    local leg = self.legs[index]
    if not leg then return nil end
    return leg.footX, leg.footY
end

function LegRig:MarkPoseDirty()
    for _, canvas in pairs(self.canvases or {}) do canvas.dirty = true end
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
                        local col = floor((x - left) / sw * tw)
                        col = clamp(col, 0, tw - 1)
                        local startRow = floor((max(fromY, top) - top) / sh * th)
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
--
-- Three stages, in order: solve the gait in CONTINUOUS world space
-- (where the no-slide math lives), quantize the result to whole texels
-- RELATIVE TO THE HIP (where the pixel-art rules live), then rasterize
-- -- but only if that quantized pose isn't the one already on the
-- canvas.
function LegRig:UpdateLegs(dt, solids)
    if not self.owner or #self.legs == 0 then return end
    solids = solids or self.solids

    local ox, oy = self.owner:GetPosition()
    local vx, vy = self.owner:GetVelocity()
    local grounded = self.owner:IsGrounded()
    local speed = abs(vx)

    local prevFacing = self.facing
    self:SetFacing(speed > self.idleSpeed and vx or nil)
    if self.facing ~= prevFacing then self:MarkPoseDirty() end

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

    -- Over one stance window the body advances stride * stanceRatio while
    -- the foot must sweep the same distance backward relative to the hip,
    -- and the sweep spans 2 * amp -- so amp is exactly half of it. Any
    -- other amplitude and planted feet slide. See Defaults.stride.
    local amp = self.stride * self.stanceRatio * 0.5 * self.blend

    local peakLift = 0.0

    for _, leg in ipairs(self.legs) do
        local hipX = ox + leg.hipX * self.facing

        local sweep, lift = gaitPose((self.phase + leg.phase) % 1.0, self.stanceRatio, self.swingFrames)
        if lift > peakLift then peakLift = lift end

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

        -- Everything from here on is integers. Offsets are taken from the
        -- HIP, not from world zero -- see this file's header for why.
        leg.soleDX = round(leg.footX - hipX)
        leg.soleDY = round(leg.footY - hipY)
    end

    -- One foot is at the top of its arc exactly when the other is
    -- straight under the body, so the swinging foot's own lift IS the
    -- body's rise -- no second phase offset to keep in sync. Rounded to
    -- whole texels; see Defaults.bob.
    local bobY = -round(self.bob * self.blend * peakLift)
    if bobY ~= self.bobY then
        self.bobY = bobY
        self:MarkPoseDirty()
    end

    -- Solve + quantize every leg, and notice whether the integer pose
    -- actually moved. `hipRow` carries the bob, so raising the body
    -- lengthens the leg toward a planted foot rather than dragging it.
    local hipRowBase = self.baseHipRow + self.bobY
    for _, leg in ipairs(self.legs) do
        self:SolveLeg(leg, self.canvasW * 0.5 + leg.hipX * self.facing, hipRowBase)
    end

    -- Rasterize only the canvases whose pose changed. Quantizing to
    -- texels is what makes this pay: standing still is a permanent skip,
    -- and even a walk holds the same integer pose for runs of frames.
    for _, layer in ipairs(LAYER_ORDER) do
        local canvas = self.canvases[layer]
        if canvas then
            if canvas.dirty then
                canvas.sprite:Clear()
                for _, leg in ipairs(self.legs) do
                    if leg.layer == layer then self:RasterizeLeg(canvas.sprite, leg) end
                end
                canvas.dirty = false
            end
            -- Position is continuous even when the raster isn't: the
            -- canvas rides the owner exactly, and quad.vert's u_PixelSnap
            -- puts the pair on the grid together.
            canvas.body:SetPosition(ox, oy + self.canvasOffY)
        end
    end
end

-- Closed-form two-bone IK, solved directly in CANVAS TEXEL SPACE. L1 is
-- the upper chain (legging + knee cap), L2 the boot; the target is the
-- ANKLE, which sits foot.height above the sole. The knee lands on the
-- circle intersection, pushed to whichever side `bend` and the current
-- facing select, and is then rounded to a whole texel like everything
-- else -- that rounding is what makes the bone's staircase hold still
-- between frames instead of crawling a pixel at a time.
function LegRig:SolveLeg(leg, hipCol, hipRow)
    local L1, L2 = leg.L1, leg.L2

    local ankleCol = hipCol + leg.soleDX
    local ankleRow = hipRow + leg.soleDY - leg.modules.foot.height

    local dx, dy = ankleCol - hipCol, ankleRow - hipRow
    local d = sqrt(dx * dx + dy * dy)
    if d < 1e-5 then dx, dy, d = 0.0, 1.0, 1.0 end

    -- Clamped just inside both singularities: exactly at full extension
    -- (or full fold) the knee's offset from the hip->ankle line is zero
    -- and its side becomes numerically undecided, which reads as a
    -- one-frame knee flip.
    local dClamped = clamp(d, abs(L1 - L2) + 0.01, L1 + L2 - 0.01)

    local ux, uy = dx / d, dy / d

    -- Distance along hip->ankle to the knee's projection, plus its
    -- perpendicular offset (law of cosines).
    local a = (L1 * L1 - L2 * L2 + dClamped * dClamped) / (2.0 * dClamped)
    local h = sqrt(max(0.0, L1 * L1 - a * a))

    -- (uy, -ux) is the perpendicular that points toward +x when the leg
    -- hangs straight down, so bend * facing puts the knee in front of
    -- the actor in whichever direction it's currently facing.
    local side = leg.bend * self.facing
    local nx, ny = uy * side, -ux * side

    local kneeCol = round(hipCol + ux * a + nx * h)
    local kneeRow = round(hipRow + uy * a + ny * h)

    -- Re-anchor the ankle onto the ROUNDED knee so the shin's drawn
    -- length matches its authored length after quantization -- otherwise
    -- rounding the knee silently stretches or shortens the bone by up to
    -- a texel, which shows up as the shin breathing while you walk.
    if L2 > 0 then
        local kx, ky = ankleCol - kneeCol, ankleRow - kneeRow
        local klen = sqrt(kx * kx + ky * ky)
        if klen > 1e-5 then
            ankleCol = round(kneeCol + kx / klen * L2)
            ankleRow = round(kneeRow + ky / klen * L2)
        end
    end

    -- Only THIS leg's canvas: a two-legged rig with one foot planted and
    -- one swinging redraws just the swinging half most frames.
    if leg.hipCol ~= hipCol or leg.hipRow ~= hipRow
        or leg.kneeCol ~= kneeCol or leg.kneeRow ~= kneeRow
        or leg.ankleCol ~= ankleCol or leg.ankleRow ~= ankleRow then
        local canvas = self.canvases[leg.layer]
        if canvas then canvas.dirty = true end
    end

    leg.hipCol, leg.hipRow = hipCol, hipRow
    leg.kneeCol, leg.kneeRow = kneeCol, kneeRow
    leg.ankleCol, leg.ankleRow = ankleCol, ankleRow
end

-- Paints one solved leg onto a canvas. Draw order is thigh, shin, knee
-- cap, foot -- the cap goes on AFTER both bones so it covers the seam
-- where their two staircases meet (which is exactly what a knee looks
-- like), and the foot goes last so it's never clipped by the shin.
function LegRig:RasterizeLeg(sprite, leg)
    local m = leg.modules
    local s = leg.shade

    local function shaded(c) return c[1] * s, c[2] * s, c[3] * s, c[4] end

    if m.legging.height > 0 or m.knee.height > 0 then
        local r, g, b, a = shaded(m.legging.color)
        sprite:DrawLimb(leg.hipCol, leg.hipRow, leg.kneeCol, leg.kneeRow, m.legging.width, r, g, b, a)
    end

    if m.boot.height > 0 then
        local r, g, b, a = shaded(m.boot.color)
        sprite:DrawLimb(leg.kneeCol, leg.kneeRow, leg.ankleCol, leg.ankleRow, m.boot.width, r, g, b, a)
    end

    if m.knee.height > 0 then
        local r, g, b, a = shaded(m.knee.color)
        sprite:FillRect(leg.kneeCol - floor(m.knee.width * 0.5),
                        leg.kneeRow - floor(m.knee.height * 0.5),
                        m.knee.width, m.knee.height, r, g, b, a)
    end

    if m.foot.height > 0 then
        local r, g, b, a = shaded(m.foot.color)
        -- Axis-aligned, always. Leaning it toward `facing` is what turns
        -- a stump into a toe -- see Defaults.footLean.
        local cx = leg.ankleCol + self.footLean * self.facing
        sprite:FillRect(cx - floor(m.foot.width * 0.5), leg.ankleRow,
                        m.foot.width, m.foot.height, r, g, b, a)
    end
end

-- layer: "back", "front", or nil for both. Split so an owner can
-- sandwich its torso between the two -- see Player:Draw().
function LegRig:DrawLegs(layer)
    for _, name in ipairs(LAYER_ORDER) do
        if layer == nil or name == layer then
            local canvas = self.canvases[name]
            if canvas then DrawBody(canvas.body) end
        end
    end
end

function LegRig:Draw()
    self:DrawLegs("back")
    self:DrawLegs("front")
end

return LegRig