-- Constants shared across game and level scripts, so tuning resolution or
-- aspect later is a one-line change rather than a search-and-replace.
local Constants = {}

-- The player's native size in texels. Every other shape, resolution and light
-- radius here is scaled against it, so the scene reads as one grid rather than
-- a pile of independently chosen sizes. Change these and re-derive the rest.
Constants.PLAYER_WIDTH = 16
Constants.PLAYER_HEIGHT = 32

-- Native pixel-art resolution: what Camera2D's viewportSize is set to. Sized
-- against the player -- 20 player-widths across, a little over 5.5 tall --
-- which is normal platformer framing rather than a tiny character lost in a
-- huge window. Exactly 16:9, and a well-worn chunky-pixel-art resolution for
-- character sizes in this range.
Constants.RESOLUTION_WIDTH = 320
Constants.RESOLUTION_HEIGHT = 180

-- Aspect the camera's content rect is letterboxed into, independent of the
-- window's own. Matches the resolution above exactly, so setting both together
-- never introduces a second, redundant letterbox.
Constants.ASPECT_WIDTH = 16
Constants.ASPECT_HEIGHT = 9

return Constants