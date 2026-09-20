-- Procedural two-bone leg rig -- the shared "legs" component for the
-- player and every NPC. Owns only its own leg canvases; the owner body
-- (torso) is passed in via SetOwner and is never moved by this class.
--
-- Each leg is four modules along one chain, plus a shared hip piece:
--
--     hip  [====]  pelvis (axis-aligned block spanning both sockets)
--          o
--          |  legging  (upper leg)
--     knee []          (joint cap -- never rotated)
--           \  boot    (lower leg)
--     ankle  o
--           [==] foot  (axis-aligned block, points the way we face)
--
-- IK is closed-form two-bone: L1 = legging.height + knee.height,
-- L2 = boot.height, solving hip -> ANKLE. The foot hangs below the ankle
-- as a rigid block, so leg.length (hip -> sole) is L1 + L2 + foot.height.
--
-- ON-GRID RENDERING: nothing here is ever rotated. The whole rig
-- rasterizes into one axis-aligned PixelSprite canvas per draw layer, and
-- only when the INTEGER pose changes. Every leg pixel is a texel, so a
-- held pose is bit-identical frame to frame. Joints are rounded relative
-- to the HIP, not to world zero -- that keeps the legs welded to the
-- torso under quad.vert's u_PixelSnap, at the cost of a planted foot
-- occasionally shifting one texel as the body crosses a texel boundary.
--
-- Ground comes from Physics.RaycastDown (Core/Physics/Raycast.h) and the
-- math kernels from the IK table (Core/Gameplay/LegIK.h). See
-- scripts/api/IK.txt for the full knob reference.

local Class = require("core.Class")

local LegRig = Class()

LegRig.Defaults = {
    legging = { width = 5, height = 9, color = {0.30, 0.33, 0.50} },
    knee    = { width = 5, height = 2, color = {0.20, 0.22, 0.34} },
    boot    = { width = 6, height = 5, color = {0.14, 0.12, 0.16} },
    foot    = { width = 7, height = 2 },   -- color defaults to boot's

    -- Pelvis / butt. Fills the crotch gap and caps the thigh tops.
    -- width nil = derived from the actual leg spread.
    -- rise nil = height - 1 (rows sitting above the hip line).
    -- rear shifts it AWAY from facing -- that is the butt.
    -- layer: "front", "back" or "both". color nil = legging color.
    -- Set hip = false, or height = 0, to remove it.
    hip = {
        width = nil, height = 4, rise = nil,
        taper = 1, rear = 1, layer = "front", color = nil,
    },

    -- Fraction of full leg length the hip rests at while WALKING, and the
    -- height the owner's collider is built around (GetStandHeight).
    -- Anything below ~0.95 keeps a permanent, stable knee bend; 1.0
    -- parks the solver on its singular fully-extended pose and leaves a
    -- swept-out foot unable to reach the ground.
    stand = 0.88,

    -- Fraction of full leg length the hip rests at while STANDING STILL.
    -- nil = same as `stand` (no change from walking). 1.0 = dead straight
    -- legs at rest. The difference is applied as a whole-texel RISE of
    -- the hip frame, carried to the torso through GetBobOffset -- the
    -- collider never moves, so the drawn body is that many texels taller
    -- than its box while idle. Capped per frame by what the planted legs
    -- can actually reach, so a foot never floats on uneven ground.
    idleStand = nil,

    -- World distance per FULL gait cycle. Sweep amplitude is DERIVED
    -- from this and stanceRatio, which is what keeps a planted foot
    -- still in world space instead of ice-skating.
    stride = 20,

    -- Fraction of the cycle each foot spends planted. Above 0.5 buys a
    -- double-support window, which is what makes a walk look planted.
    stanceRatio = 0.58,

    -- Discrete poses the SWINGING foot may occupy. Gives the crisp pop
    -- of drawn sprite animation instead of a foot melting forward.
    -- Stance is never quantized. 0 = fully continuous.
    swingFrames = 4,

    stepHeight = 3,   -- peak lift of a swinging foot, texels
    idleSpeed  = 4,   -- |vx| below this counts as standing still
    gaitBlend  = 8,   -- how fast the walk cycle fades in/out

    -- =================================================================
    -- SPRINT / CROUCH
    -- =================================================================
    -- Selected each frame by whoever drives this rig (Player:HandleInput
    -- reading Shift/Ctrl, an NPC's own AI, ...) via SetGaitState("walk" /
    -- "sprint" / "crouch"). strideScale/stepHeightScale multiply `stride`
    -- and `stepHeight` above; sink is a whole-texel amount the hip gets
    -- pulled DOWN while crouching (bent knees, torso sinking toward the
    -- ground -- the same trick GetBobOffset already uses for landing/
    -- idle, just fed a third input). All three blend in/out over
    -- gaitTuneSpeed rather than snapping, same Approach-based easing as
    -- everything else in this rig.
    sprint = { strideScale = 1.35, stepHeightScale = 1.25 },
    crouch = { strideScale = 0.55, stepHeightScale = 0.45, sink = 3 },
    gaitTuneSpeed = 10,

    -- Peak rise of the hips at mid-stance, WHOLE TEXELS. Driven off the
    -- swinging foot's own lift, so the two can't drift apart. 0 disables.
    bob = 1,

    footLean = 1,     -- texels the foot block sits forward of the ankle

    -- Foot roll. footLean is the TOE-OFF end of the roll; heelLean is
    -- the heel-strike end, so the block travels backward-to-forward
    -- under the ankle across stance instead of sitting at one offset.
    heelLean   = 2,
    pushHeight = 2,   -- texels the ankle rises while the toe stays planted
    pushToe    = 2,   -- texels the foot block narrows to during push-off

    -- =================================================================
    -- WEIGHT SHIFTING
    -- =================================================================
    -- Everything here moves the HIP, never the foot. A planted sole is
    -- anchored to the ground for the whole of stance; the body is what
    -- rises, drops, tilts and leans over it.
    weight = {
        -- Texels the hip drops on the leg carrying the body -- the
        -- stance knee flexing under load. The single biggest "has mass"
        -- cue in the rig. 1 is plenty at 32px; 2 reads as heavy/armoured.
        loadDip = 1,

        -- PELVIC TILT: texels the UNLOADED hip socket drops below the
        -- loaded one. This is the opposite of intuition and it is what
        -- real gait does -- the swing-side pelvis drops because nothing
        -- is holding it up. Sums to zero across the legs, so it tilts
        -- the pelvis without moving the body.
        tilt = 1,

        -- Landing absorb. Captured from the PREVIOUS frame's vy, because
        -- collision has already zeroed the real one by the time the legs
        -- update.
        landDip     = 2,
        landRecover = 14,
        landSpeed   = 200,  -- |vy| at impact producing a full landDip

        -- Horizontal lean. Shifts the hips AND the torso together, so
        -- the body moves over the planted feet rather than the legs
        -- sliding under a fixed torso. 0 disables.
        lean      = 2,      -- max texels
        leanSpeed = 25,     -- |vx| at which full lean is reached
        leanRate  = 9,
    },

    -- How fast a foot eases across a DISCONTINUITY (a step up, a facing
    -- flip, landing). Not a lag filter on position -- continuous motion
    -- is tracked exactly; only the leftover offset decays.
    smoothing = 24,

    -- How far below full extension to keep searching for ground.
    snapDistance = 6,

    -- Airborne pose: tucked while rising, reaching while falling.
    airTuck  = 0.68,
    airReach = 0.97,

    -- hipX is in LOCAL texels, mirrored by facing. phase offsets the leg
    -- in the gait cycle (0.5 = opposed). layer picks which canvas, and
    -- therefore which side of the torso. shade is baked into the
    -- rasterized pixels (not the body tint, which can't tell two legs on
    -- one canvas apart) to fake depth on the far leg.
    --
    -- The two stock legs are staggered one texel each way rather than
    -- sharing a hip: with both at hipX = 0 the sweep is the only thing
    -- separating them, so a standing character's silhouette collapses to
    -- a single leg.
    legs = {
        { hipX =  1, phase = 0.0, layer = "front", shade = 1.00 },
        { hipX = -1, phase = 0.5, layer = "back",  shade = 0.70 },
    },
}

-- Back first: the owner draws its torso between the two.
local LAYER_ORDER = { "back", "front" }

local floor, ceil, abs, max, min = math.floor, math.ceil, math.abs, math.max, math.min

local function clamp(v, lo, hi)
    if v < lo then return lo elseif v > hi then return hi end
    return v
end

-- Round-half-up. The same rule quad.vert's u_PixelSnap and LegIK's
-- RoundTexel use, so the three never disagree about which texel is meant.
local function round(v)
    return floor(v + 0.5)
end

local function pick(a, b)
    if a == nil then return b end
    return a
end

local function texels(v, minimum)
    v = floor((v or 0) + 0.5)
    if v < minimum then v = minimum end
    return v
end

local function makeModule(src, def, fallbackColor)
    src = src or {}
    local c = src.color or def.color or fallbackColor or {1, 1, 1, 1}
    local w = texels(pick(src.width, def.width), 1)
    return {
        width  = w,
        height = texels(pick(src.height, def.height), 0), -- 0 = module omitted
        -- nil endWidth means no taper, so an older config that only set
        -- `width` still draws as the constant-width bar it always did.
        endWidth = texels(pick(src.endWidth, def.endWidth) or w, 1),
        swell    = pick(src.swell, def.swell) or 0.0,
        swellAt  = pick(src.swellAt, def.swellAt) or 0.35,
        color  = { c[1] or 1.0, c[2] or 1.0, c[3] or 1.0, c[4] or 1.0 },
    }
end

-- ---------------------------------------------------------------------
-- Construction
-- ---------------------------------------------------------------------

-- config == false (or an empty `legs` list) builds a valid but legless
-- rig: GetStandHeight() is 0 and every per-frame method is a no-op.
function LegRig.new(config)
    local self = setmetatable({}, LegRig)
    self:InitLegRig(config)
    return self
end

-- Split out from new() so a subclass can build ITS part first and then
-- initialize the inherited leg part onto the same table. Safe to call
-- again to rebuild the rig from a new config.
function LegRig:InitLegRig(config)
    if config == false then config = { legs = {} } end
    config = config or {}

    local D = LegRig.Defaults

    self.stand         = pick(config.stand, D.stand)
    self.stride        = pick(config.stride, D.stride)
    self.stanceRatio   = clamp(pick(config.stanceRatio, D.stanceRatio), 0.05, 0.95)
    self.swingFrames   = pick(config.swingFrames, D.swingFrames)
    self.stepHeight    = pick(config.stepHeight, D.stepHeight)
    self.idleSpeed     = pick(config.idleSpeed, D.idleSpeed)
    self.gaitBlendRate = pick(config.gaitBlend, D.gaitBlend)
    self.smoothing     = pick(config.smoothing, D.smoothing)
    self.snapDistance  = pick(config.snapDistance, D.snapDistance)
    self.airTuck       = pick(config.airTuck, D.airTuck)
    self.airReach      = pick(config.airReach, D.airReach)
    self.bob           = texels(pick(config.bob, D.bob), 0)
    self.footLean      = texels(pick(config.footLean, D.footLean), 0)

    local sprintCfg, crouchCfg = config.sprint or {}, config.crouch or {}
    self.sprintTuning = {
        strideScale     = pick(sprintCfg.strideScale, D.sprint.strideScale),
        stepHeightScale = pick(sprintCfg.stepHeightScale, D.sprint.stepHeightScale),
    }
    self.crouchTuning = {
        strideScale     = pick(crouchCfg.strideScale, D.crouch.strideScale),
        stepHeightScale = pick(crouchCfg.stepHeightScale, D.crouch.stepHeightScale),
        sink            = texels(pick(crouchCfg.sink, D.crouch.sink), 0),
    }
    self.gaitTuneSpeed = pick(config.gaitTuneSpeed, D.gaitTuneSpeed)

    -- "walk" is the identity state -- scale 1.0, no sink.
    self.gaitState    = "walk"
    self.gaitScaleCur = 1.0
    self.stepScaleCur = 1.0
    self.crouchCur    = 0.0

    local baseModules = {
        legging = makeModule(config.legging, D.legging),
        knee    = makeModule(config.knee,    D.knee),
        boot    = makeModule(config.boot,    D.boot),
    }
    -- Feet inherit the boot's color, so a pre-foot config grows a
    -- matching one instead of a mismatched block.
    baseModules.foot = makeModule(config.foot, D.foot, baseModules.boot.color)
    self.modules = baseModules

    self.phase     = 0.0
    self.blend     = 0.0
    self.facing    = 1
    self.bobY      = 0
    self.owner     = nil
    self.hipLocalY = 0.0

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
            -- +1 bends the knee toward the facing direction (human);
            -- -1 bends it backward (bird / digitigrade).
            bend  = def.bend or 1,
            modules = m,
            footX = nil, footY = nil,
        }

        leg.L1     = m.legging.height + m.knee.height
        leg.L2     = m.boot.height
        leg.chain  = leg.L1 + leg.L2
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

    -- Hip piece. Built after the legs because its default width is
    -- derived from where the sockets actually ended up.
    local hcfg = config.hip
    if hcfg == false then hcfg = { height = 0 } end
    hcfg = hcfg or {}
    local hd = D.hip

    local spread = 0
    for _, leg in ipairs(self.legs) do
        local reach = abs(leg.hipX) + leg.modules.legging.width * 0.5
        if reach > spread then spread = reach end
    end

    local hipHeight = texels(pick(hcfg.height, hd.height), 0)
    local hipColor  = hcfg.color or hd.color or baseModules.legging.color

    self.hip = {
        height = hipHeight,
        width  = texels(pick(hcfg.width, hd.width) or (spread * 2 + 2), 1),
        rise   = texels(pick(hcfg.rise, hd.rise) or max(0, hipHeight - 1), 0),
        taper  = texels(pick(hcfg.taper, hd.taper), 0),
        rear   = texels(pick(hcfg.rear, hd.rear), 0),
        layer  = hcfg.layer or hd.layer or "front",
        color  = { hipColor[1] or 1.0, hipColor[2] or 1.0, hipColor[3] or 1.0, hipColor[4] or 1.0 },
    }
    -- A taper deeper than half the block would invert it partway down.
    self.hip.taper = min(self.hip.taper, floor((self.hip.width - 1) * 0.5))
    if self.hip.rise > self.hip.height then self.hip.rise = self.hip.height end

    local W = D.weight
    local wcfg = config.weight or {}
    self.loadDip     = pick(wcfg.loadDip, W.loadDip)
    self.hipTilt     = pick(wcfg.tilt, W.tilt)
    self.landDip     = pick(wcfg.landDip, W.landDip)
    self.landRecover = pick(wcfg.landRecover, W.landRecover)
    self.landSpeed   = max(1, pick(wcfg.landSpeed, W.landSpeed))
    self.leanMax     = texels(pick(wcfg.lean, W.lean), 0)
    self.leanSpeed   = max(1, pick(wcfg.leanSpeed, W.leanSpeed))
    self.leanRate    = pick(wcfg.leanRate, W.leanRate)

    self.heelLean   = texels(pick(config.heelLean, D.heelLean), 0)
    self.pushHeight = texels(pick(config.pushHeight, D.pushHeight), 0)
    self.pushToe    = texels(pick(config.pushToe, D.pushToe), 0)

    -- Canvas headroom. The hip can rise by bob + tilt and drop by
    -- loadDip + landDip; the sole never moves, so only the top needs the
    -- rise and only the pelvis shear needs the extra row.
    self.maxDip    = texels(self.loadDip + self.landDip, 0)
    self.tiltSlack = texels(self.hipTilt, 0) + ((hipHeight > 0) and 1 or 0)

    self.landCur, self.leanF, self.leanX = 0.0, 0.0, 0
    self.wasGrounded, self.lastVY, self.lastVX = true, 0.0, 0.0

    -- Pelvis column table: how many rows each column of the block spans,
    -- given the taper. Baked once so the sheared rasterizer is one
    -- FillRect per column with no per-frame width math.
    self.hipColRows = {}
    if hipHeight > 0 then
        local hw, ht = self.hip.width, self.hip.taper
        local cc = (hw - 1) * 0.5
        for c = 0, hw - 1 do
            local dist = abs(c - cc)
            local rows = 0
            for i = 0, hipHeight - 1 do
                local t = (hipHeight > 1) and (i / (hipHeight - 1)) or 0.0
                local wi = max(1, hw - round(2 * ht * t))
                if dist <= (wi - 1) * 0.5 then rows = i + 1 end
            end
            self.hipColRows[c] = rows
        end
    end

    -- Whole texels: an owner subtracts this from its total height to
    -- size its torso, and a fractional value there means a fractional
    -- torso sprite, which Sprite.NewSolid can't author.
    self.standHeight = texels(self.legLength * self.stand, 0)

    -- Idle straightening. DERIVED, not tuned: the whole-texel gap between
    -- the walking hip height (which the collider is built around) and the
    -- idle one. Whole texels because it is folded into bobY, and a
    -- fractional rise would put the torso and legs on different subpixel
    -- offsets. Never negative -- idleStand below stand is ignored rather
    -- than sinking the body into its own collider.
    local idleStand = clamp(pick(config.idleStand, D.idleStand) or self.stand, 0.0, 1.0)
    self.idleLift = max(0, texels(self.legLength * idleStand, 0) - self.standHeight)
    self.idleCur  = 0.0

    self:BuildCanvases()
end

-- ---------------------------------------------------------------------
-- Canvas allocation
-- ---------------------------------------------------------------------

-- One PixelSprite per draw layer that actually has something on it,
-- sized once to the worst-case footprint the solver can produce and
-- reused forever after.
--
-- WIDTH stacks four independent reaches: the gait sweep, the knee's
-- sideways bulge when the leg folds, half the widest module, and the
-- pelvis with its rear offset. The bulge is MEASURED (IK.KneeBulge)
-- rather than bounded, because the closed-form bound overestimates the
-- stock player by more than half and every texel costs the lighting pass.
--
-- BOTH DIMENSIONS ARE FORCED EVEN, and that is load-bearing: the canvas
-- body is centered on the owner's position, so an odd size puts its edge
-- on a half-texel and offsets the canvas's pixel grid from the torso's,
-- which makes the hip seam crawl.
function LegRig:BuildCanvases()
    self.canvases = {}
    if #self.legs == 0 then
        self.canvasW, self.canvasH, self.baseHipRow, self.canvasOffY = 0, 0, 0, 0
        return
    end

    -- Reserve for the WIDEST stride this rig can reach, which is sprint's,
    -- not the walk baseline -- otherwise a sprinting stride would sweep
    -- past the canvas edge and clip.
    local maxAmp = self.stride * self.sprintTuning.strideScale * self.stanceRatio * 0.5
    local halfW, maxLen = 0, 0

    for _, leg in ipairs(self.legs) do
        local m = leg.modules
        -- A tapered module can be widest at either end, and the swell
        -- adds to whichever that is.
        local function span(mod) return max(mod.width, mod.endWidth) + ceil(mod.swell or 0) end
        local widest = max(span(m.legging), m.knee.width, span(m.boot), m.foot.width)
        -- The target clamp in UpdateLegs never lets a sole rise above
        -- quarter extension, which is the shortest base the IK triangle
        -- can actually be handed.
        local dMin = max(abs(leg.L1 - leg.L2), leg.length * 0.25 - m.foot.height)
        local reach = abs(leg.hipX) + maxAmp + IK.KneeBulge(leg.L1, leg.L2, dMin)
                      + widest * 0.5 + self.footLean + 1
        if reach > halfW then halfW = reach end
        if leg.length > maxLen then maxLen = leg.length end
    end

    local hip = self.hip
    if hip.height > 0 then
        local hipReach = hip.width * 0.5 + hip.rear + 1
        if hipReach > halfW then halfW = hipReach end
    end
    halfW = halfW + self.leanMax   -- the hips travel sideways under lean

    local w = ceil(halfW) * 2
    if w % 2 == 1 then w = w + 1 end

    -- Rows above the hip line: bob, idle straightening, pelvic tilt (plus
    -- a row for the shear), and the pelvis itself. bob and idleLift are
    -- reserved as a SUM rather than a max: the idle rise is smoothed
    -- separately from the gait blend, so for a few frames around a
    -- start/stop the two can overlap. Rows below run to the sole, which
    -- is anchored at baseHipRow and does NOT move with the dip.
    self.baseHipRow = 1 + self.bob + self.idleLift + self.tiltSlack
                      + ((hip.height > 0) and hip.rise or 0)
    -- + crouch sink: the hip pulls further DOWN (toward the sole) while
    -- crouching, which needs the same extra row budget a taller maxLen
    -- would.
    local h = self.baseHipRow + maxLen + 2 + self.crouchTuning.sink
    if h % 2 == 1 then h = h + 1 end

    self.canvasW, self.canvasH = w, h

    local used = {}
    for _, leg in ipairs(self.legs) do used[leg.layer] = true end
    -- The pelvis can be the only thing on its layer, so it gets a vote.
    if hip.height > 0 then
        if hip.layer == "both" then
            used.front, used.back = true, true
        else
            used[hip.layer] = true
        end
    end

    for _, layer in ipairs(LAYER_ORDER) do
        if used[layer] then
            local sprite = Sprite.NewSolid(w, h, 0, 0, 0, 0)
            local body = RigidBody2D.new(0, 0, w, h)
            body:SetSprite(sprite)
            body:SetMass(0)
            -- Not a ground target: feet must never snap onto their own
            -- canvas. It has no collision shape either, so this is belt
            -- and braces against a future shape being attached.
            if body.SetRaycastTarget then body:SetRaycastTarget(false) end
            -- White tint: per-leg shade is baked into the pixels instead,
            -- because one canvas can carry several legs.
            body:SetColor(1.0, 1.0, 1.0, 1.0)
            self.canvases[layer] = { sprite = sprite, body = body, dirty = true }
        end
    end
end

-- ---------------------------------------------------------------------
-- Wiring
-- ---------------------------------------------------------------------

-- hipLocalY is the hip's offset from the OWNER BODY'S CENTER, +y down --
-- normally the torso's bottom edge (torsoHeight / 2). Rounded to a whole
-- texel: it anchors the canvas's pixel grid to the owner's, and half a
-- texel of offset there is exactly the hip-seam crawl the even-size rule
-- in BuildCanvases exists to prevent.
function LegRig:SetOwner(body, hipLocalY)
    self.owner = body
    self.hipLocalY = round(hipLocalY or 0.0)

    -- Integer by construction (integer hipLocalY, integer baseHipRow,
    -- even canvasH), which keeps torso and legs on the same grid.
    self.canvasOffY = self.hipLocalY - self.baseHipRow + self.canvasH * 0.5

    for _, leg in ipairs(self.legs) do
        leg.footX, leg.footY = nil, nil -- re-snap on the next update
        leg.groundY, leg.lastMode, leg.lastFacing = nil, nil, nil
    end
    self:MarkPoseDirty()
end

function LegRig:GetStandHeight() return self.standHeight or 0 end
function LegRig:GetLegLength()   return self.legLength or 0 end
function LegRig:HasLegs()        return #self.legs > 0 end

-- Whole-texel vertical offset the owner should draw its torso at this
-- frame, so the body rides the walk (and rises onto straight legs at
-- idle). Negative is up.
function LegRig:GetBobOffset() return self.bobY or 0 end

function LegRig:SetFacing(f)
    if f and f ~= 0 then self.facing = (f > 0) and 1 or -1 end
end
function LegRig:GetFacing() return self.facing end

-- Switches the walk/sprint/crouch gait tuning -- call every frame from
-- whatever drives this rig, e.g. Player:HandleInput reading Shift/Ctrl.
-- Not applied instantly: UpdateLegs blends stride/step/sink toward
-- whatever state is current here at gaitTuneSpeed, so flipping states
-- mid-stride eases rather than pops.
function LegRig:SetGaitState(state)
    self.gaitState = state or "walk"
end

-- World-space SOLE position (where the foot meets the ground), not the
-- ankle -- the point gameplay cares about for footstep dust and sound.
function LegRig:GetFootPosition(index)
    local leg = self.legs[index]
    if not leg then return nil end
    return leg.footX, leg.footY
end

function LegRig:MarkPoseDirty()
    for _, canvas in pairs(self.canvases or {}) do canvas.dirty = true end
end

-- ---------------------------------------------------------------------
-- Per-frame
-- ---------------------------------------------------------------------

-- Named UpdateLegs/DrawLegs rather than Update/Draw so a subclass can
-- define its own without shadowing these. Call AFTER the owner's
-- collision has resolved -- the legs react to where the torso ended up,
-- they don't predict it.
--
-- Three stages: solve the gait in CONTINUOUS world space (where the
-- no-slide math lives), quantize to whole texels RELATIVE TO THE HIP
-- (where the pixel-art rules live), then rasterize -- but only if that
-- quantized pose isn't the one already on the canvas.
--
-- Extra arguments are ignored; ground no longer comes from a caller
-- supplied list, so an older `self:UpdateLegs(dt, solids)` call is safe.
function LegRig:UpdateLegs(dt)
    if not self.owner or #self.legs == 0 then return end

    local ox, oy = self.owner:GetPosition()
    local vx, vy = self.owner:GetVelocity()
    local grounded = self.owner:IsGrounded()
    local speed = abs(vx)

    local prevFacing = self.facing
    self:SetFacing(speed > self.idleSpeed and vx or nil)
    if self.facing ~= prevFacing then self:MarkPoseDirty() end

    -- Sprint/crouch tuning, blended toward whatever SetGaitState last set
    -- -- see LegRig.Defaults' SPRINT/CROUCH section for what each target
    -- means. "walk" (the default) targets the identity values, so a rig
    -- that never calls SetGaitState behaves exactly as before.
    local targetStrideScale, targetStepScale, targetSink = 1.0, 1.0, 0.0
    if self.gaitState == "sprint" then
        targetStrideScale, targetStepScale = self.sprintTuning.strideScale, self.sprintTuning.stepHeightScale
    elseif self.gaitState == "crouch" then
        targetStrideScale, targetStepScale, targetSink =
            self.crouchTuning.strideScale, self.crouchTuning.stepHeightScale, self.crouchTuning.sink
    end
    self.gaitScaleCur = IK.Approach(self.gaitScaleCur, targetStrideScale, self.gaitTuneSpeed, dt)
    self.stepScaleCur = IK.Approach(self.stepScaleCur, targetStepScale, self.gaitTuneSpeed, dt)
    self.crouchCur    = IK.Approach(self.crouchCur, targetSink, self.gaitTuneSpeed, dt)

    local stride     = self.stride * self.gaitScaleCur
    local stepHeight = self.stepHeight * self.stepScaleCur

    if grounded and speed > self.idleSpeed then
        self.phase = (self.phase + (speed * dt) / stride) % 1.0
        self.blend = IK.Approach(self.blend, 1.0, self.gaitBlendRate, dt)
    else
        self.blend = IK.Approach(self.blend, 0.0, self.gaitBlendRate, dt)
    end

    -- Landing absorb. Fired on the grounded transition off LAST frame's
    -- vy -- collision resolves before the legs update, so the real
    -- impact velocity has already been zeroed by the time we see it.
    if grounded and not self.wasGrounded then
        self.landCur = self.landDip * clamp(abs(self.lastVY) / self.landSpeed, 0.0, 1.0)
    end
    self.landCur = IK.Approach(self.landCur, 0.0, self.landRecover, dt)
    self.wasGrounded, self.lastVY = grounded, vy

    -- Lean, from velocity rather than acceleration: acceleration spikes
    -- to whatever one frame of input sets, which reads as a twitch, and
    -- the Approach lag below already gives the lean its own overshoot on
    -- starts and stops.
    if self.leanMax > 0 then
        local target = self.leanMax * clamp(vx / self.leanSpeed, -1.0, 1.0)
        self.leanF = IK.Approach(self.leanF, grounded and target or 0.0, self.leanRate, dt)
        local leanX = round(self.leanF)
        if leanX ~= self.leanX then self.leanX = leanX; self:MarkPoseDirty() end
    end

    local airFactor = self.airTuck
    if not grounded then
        local t = clamp(vy / 200.0, -1.0, 1.0)
        airFactor = self.airTuck + (self.airReach - self.airTuck) * ((t + 1.0) * 0.5)
    end

    local hipY = oy + self.hipLocalY
    local amp  = stride * self.stanceRatio * 0.5 * self.blend
    local peakLift, totalLoad = 0.0, 0.0

    -- Smallest whole-texel reach left over across the PLANTED legs: how
    -- far the hip frame may rise before one of them would have to leave
    -- the ground. Bounds the idle straightening below.
    local minSlack = math.huge

    -- The whole per-leg WORLD-SPACE solve -- gait sample, ground
    -- raycast, target clamp, discontinuity smoothing -- is one call into
    -- IK.SolveLegFrame instead of four (IK.GaitPose, Physics.RaycastDown,
    -- two IK.Approach). See its comment in ScriptBindings.cpp for the
    -- exact argument order; it mirrors this loop's old body 1:1; only
    -- the boundary-crossing count changed, not the arithmetic.
    for _, leg in ipairs(self.legs) do
        local m = leg.modules
        local hipX = ox + leg.hipX * self.facing

        local load, lift, ankleLift, footW, footOff, footX, footY, groundY,
              offX, offY, soleDX, soleDY, lastMode, slack =
            IK.SolveLegFrame(
                (self.phase + leg.phase) % 1.0, self.stanceRatio, self.swingFrames,
                hipX, hipY, self.facing, amp,
                leg.length, m.foot.width,
                grounded, self.snapDistance, self.owner,
                self.blend, stepHeight, airFactor, self.airReach,
                self.pushHeight, self.pushToe, self.footLean, self.heelLean,
                self.smoothing, dt,
                leg.footX ~= nil, leg.footX, leg.footY, leg.offX, leg.offY,
                leg.groundY ~= nil, leg.groundY,
                leg.lastFacing, leg.lastMode)

        if lift > peakLift then peakLift = lift end
        leg.load = load
        totalLoad = totalLoad + load

        -- Heel-off/toe-narrow and foot-roll offset -- see IK.txt section
        -- 11 for what these two mean; the shaping itself now happens
        -- inside SolveLegFrame.
        leg.ankleLift = ankleLift
        leg.footW = footW
        leg.footOff = footOff

        leg.groundY = groundY -- nil when airborne this frame
        leg.offX, leg.offY = offX, offY
        leg.lastFacing, leg.lastMode = self.facing, lastMode
        leg.footX, leg.footY = footX, footY
        leg.soleDX, leg.soleDY = soleDX, soleDY

        -- slack is +inf when this leg isn't grounded this frame, so this
        -- already behaves like the old `if mode == "ground" then ...`.
        if slack < minSlack then minSlack = slack end
    end

    -- Weight distribution. For a two-legged opposed gait the loads
    -- already sum to 1; the divide is there for odd rigs and for the
    -- airborne case where every load is 0.
    local n = #self.legs
    local dipSum = 0.0
    for _, leg in ipairs(self.legs) do
        leg.share = (totalLoad > 1e-4) and (leg.load / totalLoad) or (1.0 / n)
        leg.dipF = self.loadDip * leg.share * self.blend
        dipSum = dipSum + leg.dipF
    end

    -- The body follows the MEAN hip drop; what is left over after the
    -- mean is removed is pelvic tilt, which therefore sums to zero and
    -- cannot secretly translate the character.
    local dipMean = dipSum / n
    for _, leg in ipairs(self.legs) do
        leg.tiltDY = round((leg.dipF - dipMean)
                     + self.hipTilt * (1.0 / n - leg.share) * n * self.blend)
    end

    -- Idle straightening: rise onto straight legs as the walk fades out.
    -- Driven off (1 - blend) so it is the exact complement of the gait,
    -- then smoothed again so a landing (blend already ~0 from the air)
    -- eases up instead of popping two texels in one frame. Grounded
    -- only -- airborne poses belong to airTuck/airReach. Capped by the
    -- planted legs' slack AFTER smoothing, so stepping down off a ledge
    -- mid-rise can never lift a foot off the ground.
    local idleTarget = 0.0
    if grounded and self.idleLift > 0 and minSlack < math.huge then
        idleTarget = self.idleLift * (1.0 - self.blend)
    end
    self.idleCur = IK.Approach(self.idleCur, idleTarget, self.gaitBlendRate, dt)
    local idleRise = clamp(self.idleCur, 0.0, max(0, minSlack))

    -- One rounding for the whole vertical: rise from the bob and the idle
    -- straightening, drop from the load, the landing absorb, and the
    -- crouch sink (crouchCur -- see the SPRINT/CROUCH section of
    -- LegRig.Defaults; it's a hip drop, same sign as dipMean/landCur).
    local rise = self.bob * self.blend * peakLift + idleRise
    local bobY = round(dipMean + self.landCur + self.crouchCur - rise)
    if bobY ~= self.bobY then
        self.bobY = bobY
        self:MarkPoseDirty()
    end

    -- HIP frame carries bob, dip, tilt and lean. SOLE frame is anchored
    -- at baseHipRow and never moves, so a planted foot stays planted
    -- while the body works over it.
    local mid = self.canvasW * 0.5
    local hipRowBase = self.baseHipRow + self.bobY
    local hipMoved = false
    for _, leg in ipairs(self.legs) do
        local socket = mid + leg.hipX * self.facing
        if self:SolveLeg(leg,
                socket + self.leanX, hipRowBase + leg.tiltDY,
                socket + leg.soleDX, self.baseHipRow + leg.soleDY) then
            hipMoved = true
        end
    end
    -- The pelvis spans sockets that may live on the other canvas.
    if hipMoved and self.hip.height > 0 then
        if self.hip.layer == "both" then
            self:MarkPoseDirty()
        else
            local c = self.canvases[self.hip.layer]
            if c then c.dirty = true end
        end
    end

    for _, layer in ipairs(LAYER_ORDER) do
        local canvas = self.canvases[layer]
        if canvas then
            if canvas.dirty then
                canvas.sprite:Clear()
                for _, leg in ipairs(self.legs) do
                    if leg.layer == layer then self:RasterizeLeg(canvas.sprite, leg) end
                end
                if self.hip.layer == layer or self.hip.layer == "both" then
                    self:RasterizeHip(canvas.sprite)
                end
                canvas.dirty = false
            end
            canvas.body:SetPosition(ox, oy + self.canvasOffY)
        end
    end
end

-- Returns true if the hip moved, so the caller knows whether the pelvis
-- (which may live on another canvas) needs repainting.
function LegRig:SolveLeg(leg, hipCol, hipRow, soleCol, soleRow)
    local fh = leg.modules.foot.height
    local ankleCol = soleCol
    local ankleRow = soleRow - fh - leg.ankleLift

    local kneeCol, kneeRow, aCol, aRow =
        IK.SolveTwoBone(hipCol, hipRow, ankleCol, ankleRow, leg.L1, leg.L2, leg.bend * self.facing)

    if leg.hipCol ~= hipCol or leg.hipRow ~= hipRow
        or leg.kneeCol ~= kneeCol or leg.kneeRow ~= kneeRow
        or leg.ankleCol ~= aCol or leg.ankleRow ~= aRow
        or leg.soleRow ~= soleRow or leg.drawnFootOff ~= leg.footOff
        or leg.drawnFootW ~= leg.footW then
        local canvas = self.canvases[leg.layer]
        if canvas then canvas.dirty = true end
    end

    local moved = (leg.hipCol ~= hipCol or leg.hipRow ~= hipRow)

    leg.hipCol,   leg.hipRow   = hipCol, hipRow
    leg.kneeCol,  leg.kneeRow  = kneeCol, kneeRow
    leg.ankleCol, leg.ankleRow = aCol, aRow
    leg.soleRow = soleRow
    leg.drawnFootOff, leg.drawnFootW = leg.footOff, leg.footW

    return moved
end

-- Draw order is thigh, shin, knee cap, foot -- the cap goes on AFTER
-- both bones so it covers the seam where their staircases meet (which is
-- what a knee looks like), and the foot last so the shin never clips it.
function LegRig:RasterizeLeg(sprite, leg)
    local m = leg.modules
    local s = leg.shade

    local function shaded(c) return c[1] * s, c[2] * s, c[3] * s, c[4] end

    if m.legging.height > 0 or m.knee.height > 0 then
        local r, g, b, a = shaded(m.legging.color)
        sprite:DrawTaperedLimb(leg.hipCol, leg.hipRow, leg.kneeCol, leg.kneeRow,
                               m.legging.width, m.legging.endWidth,
                               m.legging.swell, m.legging.swellAt, r, g, b, a)
    end

    if m.boot.height > 0 then
        local r, g, b, a = shaded(m.boot.color)
        sprite:DrawTaperedLimb(leg.kneeCol, leg.kneeRow, leg.ankleCol, leg.ankleRow,
                               m.boot.width, m.boot.endWidth,
                               m.boot.swell, m.boot.swellAt, r, g, b, a)
    end

    if m.knee.height > 0 then
        local r, g, b, a = shaded(m.knee.color)
        sprite:FillRect(leg.kneeCol - floor(m.knee.width * 0.5),
                        leg.kneeRow - floor(m.knee.height * 0.5),
                        m.knee.width, m.knee.height, r, g, b, a)
    end

    if m.foot.height > 0 then
        local r, g, b, a = shaded(m.foot.color)
        -- Spans ankle to sole rather than sitting at a fixed height, so
        -- a heel-off pose has no gap between the shin and the toe.
        local top = leg.ankleRow
        local h = max(1, leg.soleRow - top)
        local w = leg.footW or m.foot.width
        local cx = leg.ankleCol + (leg.footOff or self.footLean) * self.facing
        sprite:FillRect(cx - floor(w * 0.5), top, w, h, r, g, b, a)
    end
end

-- Sheared vertical column runs rather than horizontal rows, so the block
-- TILTS with the pelvis.
--
-- Every number here is computed in FORWARD-LOCAL space (+1 = the way we
-- face) and mirrored to canvas columns only on the final line. That is
-- load-bearing, not tidiness: round() is round-half-up, which is NOT
-- symmetric under negation, so any .5 rounded in canvas space biases one
-- facing and not the other. Rounding forward-local and mirroring
-- integers makes walking left an exact mirror of walking right.
-- The whole per-column shear loop (up to hip.width separate FillRects)
-- is one call into IK.RasterizeHip -- see its comment in
-- ScriptBindings.cpp. It reads leg.hipX/leg.hipRow straight off
-- self.legs and self.hipColRows straight off self, so the only things
-- left to compute here are the ones IK.RasterizeHip doesn't own: the
-- guard clauses and the color unpack.
function LegRig:RasterizeHip(sprite)
    local h = self.hip
    if h.height <= 0 then return end
    if #self.legs == 0 then return end

    IK.RasterizeHip(sprite, self.legs, h.width, h.rear, h.rise,
                     self.canvasW, self.leanX, self.facing,
                     h.color[1], h.color[2], h.color[3], h.color[4],
                     self.hipColRows)
end

-- layer: "back", "front", or nil for both. Split so an owner can
-- sandwich its torso between the two.
function LegRig:DrawLegs(layer)
    for _, name in ipairs(LAYER_ORDER) do
        if layer == nil or name == layer then
            local canvas = self.canvases[name]
            if canvas then DrawBody(canvas.body) end
        end
    end
end

-- Whole-texel HORIZONTAL offset the owner should draw its torso at this
-- frame. The hip sockets already carry the same number, so the torso and
-- the legs lean together and the planted feet stay put.
function LegRig:GetLeanOffset() return self.leanX or 0 end

function LegRig:Draw()
    self:DrawLegs("back")
    self:DrawLegs("front")
end

return LegRig