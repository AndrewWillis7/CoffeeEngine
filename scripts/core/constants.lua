-- Engine-wide constants shared across game/level scripts. Centralized
-- here rather than left as magic numbers scattered through object/level
-- code, so tuning resolution/aspect later is a one-line change instead
-- of a search-and-replace.
local Constants = {}

-- Native pixel-art resolution -- what Camera2D's viewportSize
-- "resolution control" knob (see Camera2D.h) is normally set to.
-- 640x360 is exactly 16:9 at a clean 2x scale of the also-common
-- 320x180, fine-grained enough for real pixel art without being so
-- large that individual pixels stop reading as "pixels" once scaled up
-- to a normal window/monitor size.
Constants.RESOLUTION_WIDTH = 640
Constants.RESOLUTION_HEIGHT = 360

-- Target aspect ratio the camera's content rect is letterboxed/
-- pillarboxed into, independent of the real window's own aspect -- see
-- Camera2D::targetAspect's header comment for the full nested-fit
-- explanation. Matches RESOLUTION_WIDTH/HEIGHT's own aspect exactly, so
-- setting both together (the normal case) never introduces a second,
-- redundant letterbox on top of the first.
Constants.ASPECT_WIDTH = 16
Constants.ASPECT_HEIGHT = 9

return Constants