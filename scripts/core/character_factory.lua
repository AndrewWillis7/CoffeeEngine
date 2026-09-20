-- Builds the concrete Character instances. Keeping the default leg-rig look
-- here rather than inline in the scene gives the player and every NPC the same
-- rig by construction, instead of two copies of a 60-line table drifting apart.
-- Pass your own legConfig to either Create* function to override per instance.

local Player = require("objects.player")
local NPC = require("objects.npc")
local PlayerColors = require("core.player_colors")

local CharacterFactory = {}

CharacterFactory.DEFAULT_LEG_CONFIG = {
    -- Thigh: fullest just below the hip, necking down toward the knee.
    legging = { width = 3, endWidth = 2, height = 9, swell = 1, swellAt = 0.15,
                color = {0.30, 0.33, 0.50} },
    -- Matches legging.endWidth so it sits flush, one row tall so it barely
    -- lengthens L1. Same colour as the thigh.
    knee    = { width = 2, height = 1, color = {0.30, 0.33, 0.50} },
    -- Shin: 1 texel at knee and ankle, 2 at the calf.
    -- L1 = 9 + 1 = 10, L2 = 10 -> balanced bones, full knee travel.
    boot    = { width = 1, endWidth = 1, height = 10, swell = 1, swellAt = 0.30,
                color = {0.14, 0.12, 0.16} },
    foot    = { width = 4, height = 2 },

    -- Narrow enough that the hip reads as a waistline continuing into the legs
    -- rather than a separate block, matching the torso's own taper. rear 1 gives
    -- shape without a bustle; taper is already at its max for width 4.
    hip     = { width = 4, height = 3, rise = 2, taper = 1, rear = 1, layer = "both" },

    legs = {
        { hipX =  1, phase = 0.0, layer = "front", shade = 1.00 },
        -- 1px legs lose the far one fast on a dark stage.
        { hipX = -1, phase = 0.5, layer = "back",  shade = 0.78 },
    },

    stand        = 0.92 ,  -- off the singular pose, stable bend
    idleStand    = 1.0,   -- standing still: dead straight legs
    stride       = 24,    -- retune against walk speed: speed / stride ~= 1.5-2
    stanceRatio  = 0.60,  -- a bit more double support = more grounded
    stepHeight   = 2,     -- low clearance, no marching
    swingFrames  = 0,     -- try continuous first; ~6 if you want the pop back
    snapDistance = 8,
    bob          = 1,

    footLean   = 1,
    heelLean   = 1,       -- smaller foot, smaller roll
    pushHeight = 2,
    pushToe    = 2,

    -- Defaults are fine; listed for visibility. See LegRig.Defaults.
    sprint = { strideScale = 1.35, stepHeightScale = 1.25 },
    crouch = { strideScale = 0.55, stepHeightScale = 0.45, sink = 3 },

    weight = {
        loadDip = 0,      -- see rounding note; body rhythm comes from bob
        tilt    = 1,      -- clean +/-1 socket offset -> visible hip sway
        landDip = 2, landRecover = 14, landSpeed = 200,
        lean      = 1,    -- more upright posture
        leanSpeed = 25,
        leanRate  = 9,
    },
}

-- Default clothing: a plain undershirt, no overshirt. See objects/torso.lua's
-- Defaults for every other knob. The colors here are only the fallback for
-- `palette = false` -- normally core/player_colors.lua supplies them.
CharacterFactory.DEFAULT_TORSO_CONFIG = {
    undershirt = { enabled = true, color = {0.75, 0.20, 0.25, 1.0}, neckline = 0.28, hem = 1.0, width = 1.0 },
    overshirt  = { enabled = false },
}

-- Shallow-copies a module config table with only its `color` replaced, so a
-- palette overrides color alone and every shape field survives.
local function withColor(base, color)
    if not color or not base then return base end
    local out = {}
    for k, v in pairs(base) do out[k] = v end
    out.color = color
    return out
end

-- Stamps a palette over a legConfig/torsoConfig pair WITHOUT mutating either,
-- so the shared DEFAULT_* tables can be recolored per character without the
-- colors bleeding between them.
local function applyPalette(legConfig, torsoConfig, palette)
    if not palette then return legConfig, torsoConfig end

    local leg = {}
    for k, v in pairs(legConfig) do leg[k] = v end
    leg.legging = withColor(legConfig.legging, palette.legging)
    leg.knee    = withColor(legConfig.knee, palette.knee or palette.legging)
    leg.boot    = withColor(legConfig.boot, palette.boot)
    leg.hip     = withColor(legConfig.hip, palette.hip or palette.legging)

    local torso = {}
    for k, v in pairs(torsoConfig) do torso[k] = v end
    if palette.skin then torso.color = palette.skin end
    torso.undershirt = withColor(torsoConfig.undershirt, palette.undershirt)
    torso.overshirt  = withColor(torsoConfig.overshirt, palette.overshirt)

    return leg, torso
end

-- palette is applied over legConfig/torsoConfig's own colors, defaulting to
-- core/player_colors.lua. Pass `false` to leave their colors untouched.
function CharacterFactory.CreatePlayer(x, y, w, h, legConfig, torsoConfig, palette)
    legConfig = legConfig or CharacterFactory.DEFAULT_LEG_CONFIG
    torsoConfig = torsoConfig or CharacterFactory.DEFAULT_TORSO_CONFIG
    if palette == nil then palette = PlayerColors end
    legConfig, torsoConfig = applyPalette(legConfig, torsoConfig, palette)
    return Player.new(x, y, w, h, legConfig, torsoConfig)
end

-- Actors.GetPlayer() resolves to the first body constructed with a
-- PlayerActorConfig, so always call CreatePlayer before any CreateNPC. Pass a
-- different palette here to give an NPC its own look on the same body shape.
function CharacterFactory.CreateNPC(x, y, w, h, legConfig, torsoConfig, palette)
    legConfig = legConfig or CharacterFactory.DEFAULT_LEG_CONFIG
    torsoConfig = torsoConfig or CharacterFactory.DEFAULT_TORSO_CONFIG
    if palette == nil then palette = PlayerColors end
    legConfig, torsoConfig = applyPalette(legConfig, torsoConfig, palette)
    return NPC.new(x, y, w, h, legConfig, torsoConfig)
end

return CharacterFactory
