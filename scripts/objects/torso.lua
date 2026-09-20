-- Scalable, humanoid-tapered torso + clothing. Owns the actual torso
-- RigidBody2D/sprite (what character.lua used to build inline), plus two
-- optional cloth layers on top of it:
--
--   BASE SILHOUETTE -- not a rectangle: tapers linearly from
--   `shoulderWidth` texels at the top row to `waistWidth` at the bottom
--   row (both centered), so the torso itself reads as a body rather than
--   a box. Baked into the sprite at construction, same as the clothing
--   below -- it never changes shape at runtime.
--
--   UNDERSHIRT -- baked directly into the torso's own sprite, because it
--   never extends past the torso block. Hugs the tapered silhouette
--   above (its own `width` is a fraction of THAT row's taper width, not
--   the bounding box), and is tunable from a full crew-neck shirt down
--   to a narrow sports-bra-like cut via `neckline` (how deep the collar
--   dips) and `hem` (how far up from the hips it's cropped).
--
--   OVERSHIRT ("cloak") -- ONE solid, opaque span per row (never two
--   disconnected strips with daylight between them), attached at the
--   shoulders and drifting toward the BACK leg's side as it falls, so it
--   reads as real cloth draped over one side of the body and covering
--   the trailing leg -- the leading leg is always drawn last (see
--   Character:Draw) and so stays visible in front of it regardless of
--   overlap. Shaded two-tone (a darker trailing edge, a lit leading one)
--   for a simple sense of volume. Can drop past the hips into leg space
--   (`length` in texels below the hip line), so unlike the undershirt it
--   can't just live on the torso sprite -- it gets its own canvas, sized
--   and positioned the same way LegRig sizes its leg canvases.
--
-- Owned by Character (objects/character.lua), which builds one instead
-- of the old inline RigidBody2D+Sprite pair, calls UpdateCloth every
-- frame (after legs, so it can read the leg rig's sway), and sandwiches
-- DrawOvershirt("back")/("front") around the torso draw the same way it
-- already sandwiches DrawLegs("back")/("front") around it.

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

    -- Texels wide at the top row (shoulders) and bottom row (waist/hip
    -- line) -- linearly interpolated row by row, centered. This is what
    -- makes the silhouette read as humanoid instead of a rectangle; every
    -- clothing layer below hugs whatever this taper is at its own row.
    shoulderWidth = 4,
    waistWidth    = 2,

    undershirt = {
        enabled = true,
        color   = {0.75, 0.20, 0.25, 1.0},
        -- Fraction of torso height the neckline dips down from the top
        -- (the shoulder line). 0 = crew neck covering up to the
        -- shoulders, toward ~0.5 = a deep scoop baring the upper chest.
        neckline = 0.28,
        -- Rows the neckline cut is spread across -- 0 is a hard
        -- horizontal cutoff, more rounds it into a shallow scoop/V.
        necklineTaper = 2,
        -- Fraction of the REMAINING height (below the neckline) the hem
        -- covers, anchored at the hips. 1.0 = a full shirt terminating
        -- exactly at the hips (the undershirt can never go lower than
        -- that -- see the module comment); smaller crops it upward
        -- toward a sports-bra/crop silhouette.
        hem = 1.0,
        -- Fraction of the BODY'S TAPER WIDTH at each row -- 1.0 hugs the
        -- silhouette exactly, smaller narrows toward a bra/tank strap
        -- width.
        width = 1.0,
    },

    overshirt = {
        enabled = false,
        -- Brown by default -- reads as cloth/leather against the torso's
        -- own colors and stays visually distinct from either.
        color = {0.36, 0.22, 0.12, 1.0},
        -- Fraction of torso height the cloak's attach line starts at --
        -- 0 drapes it from the very top of the shoulders.
        neckline = 0.0,
        -- Half-width of EACH PANEL at the shoulder attachment, texels --
        -- two panels (left/right of `openFront`'s gap, like an
        -- unbuttoned trench coat's flaps). Matches Torso.Defaults'
        -- shoulderWidth/2 (4/2 = 2) so the top edge sits FLUSH against
        -- the torso's own shoulder edge instead of already being wider
        -- than the body it's supposedly attached to -- retune this
        -- alongside shoulderWidth if that changes.
        width = 2,
        -- Fraction of each panel's own half-width that peels away as a
        -- gap down the middle, growing from 0 at `gapStart` to this
        -- fraction by the hem -- what shows the undershirt through
        -- rather than a sealed, buttoned-up front. Not scaled by motion:
        -- an unbuttoned coat is open at rest too.
        openFront = 0.5,
        -- Fraction of the coat's length (from the collar) that stays
        -- fully CLOSED/connected -- like a real trench coat's lapels,
        -- which overlap right at the neckline before the flaps actually
        -- separate. Below this point the gap ramps in linearly to
        -- `openFront`'s full width by the hem; above it there is no gap
        -- at all, the two panels' edges meet. Kept small and measured
        -- against the WHOLE coat span (torso + below-hip length
        -- combined, see RasterizeOvershirt) -- with the torso alone
        -- being ~60% of that span, anything much bigger than this
        -- swallows the entire torso and leaves no undershirt visible.
        gapStart = 0.12,
        -- Texels the half-width grows by the hem, on top of `width` --
        -- a gentle, SMOOTH taper across the whole drop (not gated to
        -- motion or to the hip line the way backDrift is below) -- a
        -- coat that suddenly stops widening partway down reads as
        -- broken, not draped.
        flare = 1.5,
        -- Texels the WHOLE span drifts toward the back leg's side BY THE
        -- HIP LINE, held there for the rest of the drop (the -facing
        -- direction -- see LegRig's own hipX*facing convention, and
        -- RasterizeOvershirt's hipT) -- what lets a shoulder-centered
        -- garment end up covering the trailing leg through its whole
        -- visible length, while the leading leg, always drawn last (see
        -- Character:Draw), stays visible in front of it regardless of
        -- overlap. Kept modest -- width+flare already brackets both legs
        -- at the hip on their own (see hipT's comment); this only needs
        -- to tip the balance toward one side, not fling the coat off the
        -- body. Scaled by motion (see restSwing).
        backDrift = 1.5,
        -- Floor on backDrift's motion scale (see UpdateCloth) -- 1.0 =
        -- always fully drifted regardless of motion, 0.0 = dead flat and
        -- centered at a full stop. There's no wind model here, so "at
        -- rest" means only gravity: thinner and flatter, not zero (real
        -- cloth still hangs with some shape).
        restSwing = 0.25,
        -- Each panel is uniformly tinted -- the one on the trailing
        -- (away-from-facing) side at `shadeMul`, the other at full color
        -- -- simple one-sided volume cue, same idea as LegRig's far-leg
        -- shade.
        shadeMul = 0.6,
        -- Texels the coat hangs BELOW the hip line. Tuned against
        -- CharacterFactory.DEFAULT_LEG_CONFIG's legging+knee height
        -- (hip-to-knee ~= 10 texels there) to land a little ABOVE the
        -- knee -- retune this if that leg config's proportions change.
        length = 8,
        -- How much of the owner's BODY LEAN carries into the cloak -- 0
        -- is rigid, 1 is full transfer. No wind and no fabricated idle
        -- flutter -- this is the only source of sway, so it stays
        -- bounded by however far the body itself actually leans (see
        -- LegRig.Defaults.weight.lean, normally +/-1 texel). Applied
        -- per-row, scaled by distance from the (anchored) collar, so the
        -- hem swings while the shoulders stay put.
        flow    = 0.4,
        flowLag = 10,   -- IK.Approach rate the sway eases toward its target at
        -- "front", "back", or "both" -- which side of the legs the coat
        -- draws on, same convention as LegRig's hip.layer.
        layer = "front",
    },
}

-- ---------------------------------------------------------------------
-- Construction
-- ---------------------------------------------------------------------

-- w, torsoHeight: the torso block's own bounding size (texels) -- the
-- SILHOUETTE drawn inside it tapers per shoulderWidth/waistWidth, so it
-- does not have to (and by design should not) fill that box. standHeight:
-- the leg rig's GetStandHeight(), currently unused but kept for parity
-- with the canvas-sizing pattern LegRig itself uses. config == false
-- disables both cloth layers but still builds the plain tapered body.
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

-- Half-width of the tapered silhouette at a given row (0 = top/shoulder
-- row, h-1 = bottom/waist row), in texels. Shared by the base silhouette
-- fill and the undershirt, so clothing always hugs the body's own taper
-- instead of a fixed rectangle.
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

    -- Neckline taper: the top `necklineTaper` rows narrow toward the
    -- collar from the body's own taper width at that row, rather than
    -- a fixed width -- the shirt hugs the silhouette even while cropping
    -- the collar.
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

    -- offY anchors the canvas so its attach row lines up with the
    -- torso's own attach line, in the owner's local space (+y down,
    -- torso center at 0) -- mirrors LegRig:SetOwner's hipLocalY
    -- bookkeeping.
    local torsoNeckLocalY = -self.h * 0.5 + neckRow
    self.coatOffY = round(torsoNeckLocalY - self.coatTopRow + h * 0.5)

    local sprite = Sprite.NewSolid(w, h, 0, 0, 0, 0)
    local body = RigidBody2D.new(0, 0, w, h)
    body:SetSprite(sprite)
    body:SetMass(0)
    if body.SetRaycastTarget then body:SetRaycastTarget(false) end
    body:SetColor(1.0, 1.0, 1.0, 1.0)
    self.coatCanvas = { sprite = sprite, body = body, dirty = true }
end

-- ---------------------------------------------------------------------
-- Per-frame
-- ---------------------------------------------------------------------

-- Call once a frame, AFTER the owning Character's legs have updated --
-- leanX/facing/gaitBlend/phase are the same signals LegRig itself just
-- computed (Character can read them straight off self, since Character
-- IS a LegRig). ox, oy is the owner body's current position.
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

    -- How flared/drifted the cloak is right now -- no wind model, so
    -- this tracks motion alone: gaitBlend is 0 standing still (flat,
    -- thin, centered -- just restSwing's floor) and 1 fully walking
    -- (the full flare/drift configured above).
    local newSwing = o.restSwing + (1.0 - o.restSwing) * clamp(gaitBlend or 0, 0.0, 1.0)
    if abs(newSwing - self.swingScale) > 0.02 then
        self.swingScale = newSwing
        canvas.dirty = true
    end

    -- No wind, no fabricated idle flutter -- sway tracks the body's own
    -- lean alone, so it stays bounded by however far the body actually
    -- leans instead of an independent oscillation that can carry the
    -- cloak away from the body it's meant to hang on.
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

-- TWO panels per row, like an unbuttoned trench coat's flaps -- a gap
-- down the middle (closed at the collar, opening wider toward the hem)
-- shows whatever's underneath (the undershirt, above the hips; legs/
-- background below them, same as a real open coat) instead of sealing it
-- off. The whole pair attaches at the shoulders and drifts toward the
-- back leg's side (the -facing direction, mirroring LegRig's own
-- hipX*facing convention) as it falls, so the trailing leg ends up under
-- it while the leading leg (always drawn last -- see Character:Draw)
-- stays visible regardless of overlap. Flare/drift are scaled by
-- self.swingScale (see UpdateCloth) -- thin and centered at rest, flared
-- open while walking. Each panel is uniformly shaded: the trailing
-- (away-from-facing) one darker, the leading one full color, for a cheap
-- sense of volume. Everything is computed relative to the CANVAS CENTER
-- (mid), not its left edge -- get that wrong and it degenerates into a
-- lopsided sliver stuck to one side instead of a centered, correctly-
-- drifting cloak.
function Torso:RasterizeOvershirt(sprite)
    local o = self.overshirtCfg
    local c = o.color
    local top, bottom = self.coatTopRow, self.coatBottomRow
    local span = max(1, bottom - top)
    local mid = self.coatCanvasW * 0.5
    local facing = self.facing or 1
    local swing = self.swingScale or o.restSwing

    -- Where the hip line falls within the coat's own span (0..1) --
    -- flare/drift/sway reach their FULL value by this point and hold
    -- there for the rest of the drop, rather than only reaching full
    -- strength at the very last row. Without this, the leg-covering
    -- portion below the hip only ever gets a fraction of the drift (most
    -- of it concentrated in the last texel or two at the hem), which
    -- isn't enough to actually overlap a leg through its swing.
    local hipT = max(0.05, (self.coatHipRow - top) / span)

    local sr, sg, sb = c[1] * o.shadeMul, c[2] * o.shadeMul, c[3] * o.shadeMul
    local trailing = { sr, sg, sb, c[4] }
    local leading  = { c[1], c[2], c[3], c[4] }
    local leftPanel, rightPanel
    if facing >= 0 then leftPanel, rightPanel = trailing, leading
    else leftPanel, rightPanel = leading, trailing end

    for row = top, bottom do
        local t = (row - top) / span
        -- flare tapers smoothly across the WHOLE drop (plain t) -- a
        -- coat that stops widening partway down looks broken, not
        -- draped. backDrift/sway use dragT (full strength by the hip,
        -- held for the rest of the drop) so the leg-covering portion
        -- gets consistent coverage instead of a weak, still-ramping
        -- offset concentrated in the last texel or two.
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

-- layer: "back", "front", or nil for both -- same convention as
-- LegRig:DrawLegs, so Character can sandwich its torso draw between
-- DrawOvershirt("back") and DrawOvershirt("front") the same way it
-- already sandwiches it between DrawLegs("back")/("front").
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
