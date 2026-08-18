-- Engine-wide constants shared across game/level scripts. Centralized
-- here rather than left as magic numbers scattered through object/level
-- code, so tuning resolution/aspect later is a one-line change instead
-- of a search-and-replace.
local Constants = {}

-- The player's native pixel-art size, in texels -- the one number every
-- other shape/resolution/light-radius choice in this file is scaled
-- relative to, so the whole scene reads as "on the same grid" instead of
-- each object's size being picked independently. Change these two
-- numbers and re-derive everything else that depends on them (see the
-- comments below) rather than hand-tuning sizes in isolation.
Constants.PLAYER_WIDTH = 16
Constants.PLAYER_HEIGHT = 32

-- Native pixel-art resolution -- what Camera2D's viewportSize
-- "resolution control" knob (see Camera2D.h) is normally set to. Sized
-- relative to PLAYER_WIDTH/HEIGHT above: 320 wide is 20 player-widths
-- across, 180 tall is a little over 5.5 player-heights -- a normal
-- platformer framing (a few character-heights of headroom/fall room
-- visible at once), not a huge window with a tiny character lost in it.
-- Still exactly 16:9, matching ASPECT_WIDTH/HEIGHT below, and happens to
-- be Camera2D's own class-default (see its header) -- no coincidence,
-- it's a well-worn "chunky pixel art" reference resolution for
-- character sizes in this range (comparable to Axiom Verge/Celeste-era
-- proportions).
Constants.RESOLUTION_WIDTH = 320
Constants.RESOLUTION_HEIGHT = 180

-- Target aspect ratio the camera's content rect is letterboxed/
-- pillarboxed into, independent of the real window's own aspect -- see
-- Camera2D::targetAspect's header comment for the full nested-fit
-- explanation. Matches RESOLUTION_WIDTH/HEIGHT's own aspect exactly, so
-- setting both together (the normal case) never introduces a second,
-- redundant letterbox on top of the first.
Constants.ASPECT_WIDTH = 16
Constants.ASPECT_HEIGHT = 9

return Constants