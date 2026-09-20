-- Scalable, humanoid-tapered torso and clothing. Owns the torso's own
-- RigidBody2D and sprite, plus two optional cloth layers:
--
--   BASE SILHOUETTE -- not a rectangle. Tapers linearly from shoulderWidth at
--   the top row to waistWidth at the bottom, both centred, so the torso reads
--   as a body rather than a box. Baked in at construction; never changes shape.
--
--   UNDERSHIRT -- baked into the torso's own sprite, since it never extends
--   past the torso block. Hugs the taper above (its `width` is a fraction of
--   THAT row's width, not the bounding box) and tunes from a crew neck down to
--   a narrow cut via `neckline` and `hem`.
--
--   OVERSHIRT ("cloak") -- one solid span per row, never two disconnected
--   strips with daylight between them. Attaches at the shoulders and drifts
--   toward the BACK leg's side as it falls, so it reads as cloth draped over
--   one side and covering the trailing leg; the leading leg is drawn last and
--   stays visible regardless of overlap. Two-tone shaded for volume. It can
--   drop past the hips into leg space, so unlike the undershirt it needs its
--   own canvas, sized the way LegRig sizes its leg canvases.
--
-- Owned by Character, which calls UpdateCloth every frame after the legs (so it
-- can read their sway) and sandwiches DrawOvershirt("back")/("front") around
-- the torso draw, exactly as it does with DrawLegs.

local Class = require("core.Class")

local Torso = Class()

local floor, ceil, abs, max, min = math.floor, math.ceil, math.abs, math.max, math.min

local function round(v) return floor(v + 0.5) end
local function clamp(v, lo, hi)
    if v < lo then return lo elseif v > hi then return hi end
    return v
end
local function pick(a, b) if a == nil then return b end return a end

Torso.Defaults = {
    color = {1.0, 1.0, 1.0, 1.0},

    -- Texels wide at the top (shoulders) and bottom (waist) rows, interpolated
    -- row by row and centred. Every clothing layer hugs this taper at its row.
    shoulderWidth = 4,
    waistWidth    = 2,

    undershirt = {
        enabled = true,
        color   = {0.75, 0.20, 0.25, 1.0},
        -- Fraction of torso height the neckline dips from the shoulder line.
        -- 0 is a crew neck, ~0.5 a deep scoop.
        neckline = 0.28,
        -- Rows the cut spreads across: 0 is a hard edge, more rounds it.
        necklineTaper = 2,
        -- Fraction of the REMAINING height below the neckline the hem covers,
        -- anchored at the hips. 1.0 ends exactly at the hips, which is as low
        -- as the undershirt can go; smaller crops it upward.
        hem = 1.0,
        -- Fraction of the body's taper width per row: 1.0 hugs it exactly.
        width = 1.0,
    },

    overshirt = {
        enabled = false,
        -- Brown reads as cloth against the torso's own colors.
        color = {0.36, 0.22, 0.12, 1.0},
        -- Fraction of torso height the attach line starts at; 0 is the shoulders.
        neckline = 0.0,
        -- Half-width of EACH panel at the shoulder, in texels -- two panels
        -- either side of openFront's gap, like an unbuttoned coat's flaps.
        -- Matches shoulderWidth/2 so the top edge sits FLUSH against the
        -- torso's shoulder rather than already overhanging it. Retune together.
        width = 2,
        -- Fraction of each panel's half-width that peels away as a centre gap,
        -- growing from 0 at gapStart to this by the hem -- what shows the
        -- undershirt through. Not motion-scaled: an open coat is open at rest.
        openFront = 0.5,
        -- Fraction of the coat's length from the collar that stays CLOSED, the
        -- way real lapels overlap before the flaps separate. Below it the gap
        -- ramps linearly to openFront by the hem; above it the panels meet.
        -- Measured against the WHOLE span, torso plus below-hip length -- and
        -- since the torso alone is ~60% of that, much more swallows it entirely.
        gapStart = 0.12,
        -- Texels the half-width grows by the hem, on top of `width`. A smooth
        -- taper across the whole drop, ungated: a coat that stops widening
        -- partway down reads as broken rather than draped.
        flare = 1.5,
        -- Texels the whole span drifts toward the back leg's side BY THE HIP
        -- LINE, held there for the rest of the drop. This is what lets a
        -- shoulder-centred garment cover the trailing leg along its whole
        -- length. Kept modest: width+flare already bracket both legs at the
        -- hip, so this only tips the balance rather than flinging the coat off
        -- the body. Motion-scaled -- see restSwing.
        backDrift = 1.5,
        -- Floor on backDrift's motion scale: 1.0 always fully drifted, 0.0 dead
        -- flat at a stop. With no wind model, "at rest" means gravity alone --
        -- thinner and flatter, but not zero, since real cloth still has shape.
        restSwing = 0.25,
        -- The trailing panel tints to shadeMul, the leading one stays full --
        -- a one-sided volume cue, the same idea as LegRig's far-leg shade.
        shadeMul = 0.6,
        -- Texels the coat hangs BELOW the hip line, tuned against the default
        -- leg config's ~10-texel hip-to-knee to land just above the knee.
        length = 8,
        -- How much of the owner's BODY LEAN carries into the cloak: 0 rigid,
        -- 1 full. With no wind and no fabricated flutter this is the only
        -- source of sway, so it stays bounded by how far the body actually
        -- leans. Applied per row, scaled by distance from the anchored collar,
        -- so the hem swings while the shoulders stay put.
        flow    = 0.4,
        flowLag = 10,   -- IK.Approach rate the sway eases toward its target at
        -- Which side of the legs the coat draws on, as LegRig's hip.layer.
        layer = "front",
    },
}

-- ---------------------------------------------------------------------
-- Construction
-- ---------------------------------------------------------------------

-- w, torsoHeight are the block's bounding size; the silhouette inside tapers
-- and by design does not fill it. standHeight is unused, kept for parity with
-- LegRig's canvas sizing. config == false disables both cloth layers but still
-- builds the plain tapered body.
function Torso.new(x, y, w, torsoHeight, standHeight, config)
    local self = setmetatable({}, Torso)
    self:Init(x, y, w, torsoHeight, standHeight, config)
    return self
end

function Torso:Init(x, y, w, torsoHeight, standHeight, config)
    if config == false then config = { undershirt = { enabled = false }, overshirt = { enabled = false } } end
    config = config or {}
    local D = Torso.Defaults

    self.w, self.h = w, torsoHeight
    self.standHeight = standHeight or 0

    local color = config.color or D.color
    self.color = { color[1] or 1.0, color[2] or 1.0, color[3] or 1.0, color[4] or 1.0 }

    self.shoulderWidth = max(1, floor(pick(config.shoulderWidth, D.shoulderWidth) + 0.5))
    self.waistWidth    = max(1, floor(pick(config.waistWidth, D.waistWidth) + 0.5))

    local ucfg = config.undershirt or {}
    local ud = D.undershirt
    self.undershirt = {
        enabled       = pick(ucfg.enabled, ud.enabled),
        color         = ucfg.color or ud.color,
        neckline      = clamp(pick(ucfg.neckline, ud.neckline), 0.0, 0.9),
        necklineTaper = max(0, floor(pick(ucfg.necklineTaper, ud.necklineTaper) + 0.5)),
        hem           = clamp(pick(ucfg.hem, ud.hem), 0.0, 1.0),
        width         = clamp(pick(ucfg.width, ud.width), 0.1, 1.5),
    }

    local ocfg = config.overshirt or {}
    local od = D.overshirt
    self.overshirtCfg = {
        enabled   = pick(ocfg.enabled, od.enabled),
        color     = ocfg.color or od.color,
        neckline  = clamp(pick(ocfg.neckline, od.neckline), 0.0, 0.9),
        width     = max(0.5, pick(ocfg.width, od.width)),
        openFront = clamp(pick(ocfg.openFront, od.openFront), 0.0, 0.95),
        gapStart  = clamp(pick(ocfg.gapStart, od.gapStart), 0.0, 0.95),
        flare     = max(0, pick(ocfg.flare, od.flare)),
        backDrift = max(0, pick(ocfg.backDrift, od.backDrift)),
        restSwing = clamp(pick(ocfg.restSwing, od.restSwing), 0.0, 1.0),
        shadeMul  = clamp(pick(ocfg.shadeMul, od.shadeMul), 0.1, 1.0),
        length    = max(0, floor(pick(ocfg.length, od.length) + 0.5)),
        flow      = clamp(pick(ocfg.flow, od.flow), 0.0, 1.5),
        flowLag   = max(0.1, pick(ocfg.flowLag, od.flowLag)),
        layer     = ocfg.layer or od.layer,
    }

    self:BuildBody(x, y)
    self:BuildOvershirtCanvas()

    self.swayX, self.swayTarget = 0, 0
    self.facing = 1
    self.swingScale = self.overshirtCfg.restSwing
end

-- Half-width of the silhouette at a row (0 = shoulders, h-1 = waist). Shared
-- by the base fill and the undershirt, so clothing hugs the body's own taper.
function Torso:TaperHalfWidthAt(row)
    local h = self.h
    local t = (h > 1) and clamp(row / (h - 1), 0.0, 1.0) or 0.0
    local width = self.shoulderWidth + (self.waistWidth - self.shoulderWidth) * t
    return width * 0.5
end

-- The rigid torso block itself (tapered silhouette), plus the undershirt
-- baked into its sprite -- neither changes shape at runtime (only the
-- coat does), so this is drawn once at construction, not every frame.
function Torso:BuildBody(x, y)
    local w, h = self.w, self.h

    self.body = RigidBody2D.new(x, y, w, h)
    self.body:SetName("Character")
    self.sprite = Sprite.NewSolid(w, h, 0, 0, 0, 0)
    self.body:SetSprite(self.sprite)

    self:RasterizeSilhouette()

    if self.undershirt.enabled then
        self:RasterizeUndershirt()
    end

    self.sprite:Flush()
end

function Torso:RasterizeSilhouette()
    local cx = self.w * 0.5
    local c = self.color
    for row = 0, self.h - 1 do
        local halfW = self:TaperHalfWidthAt(row)
        local rw = max(1, round(halfW * 2))
        local left = round(cx - halfW)
        self.sprite:FillRect(left, row, rw, 1, c[1], c[2], c[3], c[4])
    end
end

function Torso:RasterizeUndershirt()
    local u = self.undershirt
    local h = self.h
    local cx = self.w * 0.5
    local c = u.color

    local neckRow = floor(u.neckline * h + 0.5)
    local hemRow = h - floor((h - neckRow) * (1.0 - u.hem) + 0.5)
    if hemRow <= neckRow then return end

    -- The top necklineTaper rows narrow toward the collar from the body's own
    -- width at that row, so the shirt hugs the silhouette while cropping.
    local taper = min(u.necklineTaper, hemRow - neckRow)
    for row = neckRow, hemRow - 1 do
        local bodyHalfW = self:TaperHalfWidthAt(row) * u.width
        local rowHalfW = bodyHalfW
        if row < neckRow + taper then
            local t = (taper > 0) and ((row - neckRow) / taper) or 1.0
            rowHalfW = bodyHalfW * t
        end
        local rw = max(0, round(rowHalfW * 2))
        if rw > 0 then
            local left = round(cx - rowHalfW)
            self.sprite:FillRect(left, row, rw, 1, c[1], c[2], c[3], c[4])
        end
    end
end

-- One canvas, sized to the cloak's own footprint (shoulder attachment to
-- hem-with-sway-and-flare), positioned around the torso the same way
-- LegRig positions its leg canvases around the hip.
function Torso:BuildOvershirtCanvas()
    self.coatCanvas = nil
    local o = self.overshirtCfg
    if not o.enabled or o.length <= 0 then return end

    local maxHalfW = o.width + o.flare
    -- Sway (either direction) stacks with the backward drift (whichever
    -- direction facing puts it in) -- reserve for the worst case on both
    -- sides so it never clips.
    local maxSway = self.w * o.flow + 2
    local halfW = ceil(maxHalfW + o.backDrift + maxSway) + 1
    local w = halfW * 2
    if w % 2 == 1 then w = w + 1 end

    local neckRow = floor(o.neckline * self.h + 0.5)
    local h = (self.h - neckRow) + o.length + 2
    if h % 2 == 1 then h = h + 1 end

    self.coatCanvasW, self.coatCanvasH = w, h
    self.coatTopRow = floor((h - ((self.h - neckRow) + o.length)) * 0.5)
    self.coatHipRow = self.coatTopRow + (self.h - neckRow)
    self.coatBottomRow = self.coatHipRow + o.length

    -- Anchors the canvas so its attach row lines up with the torso's, in the
    -- owner's local space -- mirrors LegRig:SetOwner's hipLocalY bookkeeping.
    local torsoNeckLocalY = -self.h * 0.5 + neckRow
    self.coatOffY = round(torsoNeckLocalY - self.coatTopRow + h * 0.5)

    local sprite = Sprite.NewSolid(w, h, 0, 0, 0, 0)
    local body = RigidBody2D.new(0, 0, w, h)
    body:SetName("Coat Canvas")
    body:SetSprite(sprite)
    body:SetMass(0)
    if body.SetRaycastTarget then body:SetRaycastTarget(false) end
    body:SetColor(1.0, 1.0, 1.0, 1.0)
    self.coatCanvas = { sprite = sprite, body = body, dirty = true }
end

-- ---------------------------------------------------------------------
-- Per-frame
-- ---------------------------------------------------------------------

-- Call once a frame, AFTER the legs update: leanX, facing, gaitBlend and phase
-- are the signals LegRig just computed, which Character reads straight off self.
function Torso:UpdateCloth(dt, ox, oy, leanX, facing, gaitBlend, phase)
    local canvas = self.coatCanvas
    if not canvas then return end
    local o = self.overshirtCfg

    -- The backward drift (and its shading) is mirrored by facing -- a
    -- flip has to repaint, same as LegRig marking its own pose dirty on
    -- a facing change.
    local newFacing = (facing and facing < 0) and -1 or 1
    if self.facing ~= newFacing then
        self.facing = newFacing
        canvas.dirty = true
    end

    -- No wind model, so flare and drift track motion alone: gaitBlend 0 is
    -- standing still at restSwing's floor, 1 is the full configured flare.
    local newSwing = o.restSwing + (1.0 - o.restSwing) * clamp(gaitBlend or 0, 0.0, 1.0)
    if abs(newSwing - self.swingScale) > 0.02 then
        self.swingScale = newSwing
        canvas.dirty = true
    end

    -- Sway tracks the body's own lean alone, so it stays bounded by the body
    -- rather than being an oscillation that can carry the cloak off it.
    self.swayTarget = (leanX or 0) * o.flow

    local prev = self.swayX
    self.swayX = IK.Approach(self.swayX, self.swayTarget, o.flowLag, dt)
    if round(self.swayX) ~= round(prev) then canvas.dirty = true end

    if canvas.dirty then
        canvas.sprite:Clear()
        self:RasterizeOvershirt(canvas.sprite)
        canvas.dirty = false
    end
    canvas.body:SetPosition(ox, oy + self.coatOffY)
end

-- Two panels per row, like an unbuttoned coat's flaps, with a centre gap that
-- is closed at the collar and opens toward the hem so the undershirt (and, past
-- the hips, the legs) shows through. The pair attaches at the shoulders and
-- drifts toward the back leg's side as it falls, putting the trailing leg under
-- it while the leading leg, drawn last, stays visible. Flare and drift scale by
-- self.swingScale: thin and centred at rest, flared open while walking. The
-- trailing panel is shaded darker for volume.
--
-- Everything is computed relative to the CANVAS CENTRE, not its left edge --
-- get that wrong and the cloak degenerates into a lopsided sliver on one side.
function Torso:RasterizeOvershirt(sprite)
    local o = self.overshirtCfg
    local c = o.color
    local top, bottom = self.coatTopRow, self.coatBottomRow
    local span = max(1, bottom - top)
    local mid = self.coatCanvasW * 0.5
    local facing = self.facing or 1
    local swing = self.swingScale or o.restSwing

    -- Where the hip line falls in the coat's own span. Flare, drift and sway
    -- reach FULL value here and hold for the rest of the drop; without that the
    -- leg-covering portion below the hip only ever gets a fraction of the
    -- drift, concentrated in the last texel or two, which can't overlap a leg.
    local hipT = max(0.05, (self.coatHipRow - top) / span)

    local sr, sg, sb = c[1] * o.shadeMul, c[2] * o.shadeMul, c[3] * o.shadeMul
    local trailing = { sr, sg, sb, c[4] }
    local leading  = { c[1], c[2], c[3], c[4] }
    local leftPanel, rightPanel
    if facing >= 0 then leftPanel, rightPanel = trailing, leading
    else leftPanel, rightPanel = leading, trailing end

    for row = top, bottom do
        local t = (row - top) / span
        -- flare uses plain t so it tapers across the WHOLE drop; backDrift and
        -- sway use dragT, full strength by the hip and held, so the
        -- leg-covering portion gets consistent coverage.
        local dragT = clamp(t / hipT, 0.0, 1.0)
        local halfW = o.width + o.flare * swing * t
        local drift = -facing * (o.backDrift * swing * dragT) + self.swayX * dragT
        local cx = mid + drift

        -- Closed/overlapping lapels down to gapStart -- gt is 0 there
        -- (gap = 0, panels meet with no seam) and ramps 0..1 from
        -- gapStart to the hem.
        local gt = (t <= o.gapStart) and 0.0 or (t - o.gapStart) / (1.0 - o.gapStart)
        local gap = halfW * o.openFront * gt
        local left, right = round(cx - halfW), round(cx + halfW)
        local gapL, gapR = round(cx - gap), round(cx + gap)

        local leftW = max(0, gapL - left)
        local rightW = max(0, right - gapR)
        if leftW > 0 then
            sprite:FillRect(left, row, leftW, 1, leftPanel[1], leftPanel[2], leftPanel[3], leftPanel[4])
        end
        if rightW > 0 then
            sprite:FillRect(gapR, row, rightW, 1, rightPanel[1], rightPanel[2], rightPanel[3], rightPanel[4])
        end
    end
end

-- layer is "back", "front", or nil for both -- the same convention as
-- LegRig:DrawLegs, so Character can sandwich its torso draw between the two.
function Torso:DrawOvershirt(layer)
    local canvas = self.coatCanvas
    if not canvas then return end
    local o = self.overshirtCfg
    if o.layer == "both" or o.layer == layer then
        DrawBody(canvas.body)
    end
end

function Torso:Draw()
    DrawBody(self.body)
end

return Torso
