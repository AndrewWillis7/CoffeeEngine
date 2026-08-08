-- Wraps a RigidBody2D + Camera2D as a single game object, same pattern
-- Player uses for RigidBody2D + PlayerActorConfig. This body's own
-- position IS the camera's world-space center -- there's nothing else to
-- it. It's never drawn (no DrawBody() call for it anywhere) and never
-- collides (no CollisionShape2D attached), it just carries a Camera2D
-- around.

local Class = require("core.Class")

local Camera = Class()

-- viewportW/H: world units visible across the full window regardless of
-- the window's actual pixel size -- this is the "resolution control"
-- knob (see Camera2D.h). Defaults to Camera2D's own default (320x180,
-- a common pixel-art virtual resolution) if you don't pass anything.
function Camera.new(x, y, viewportW, viewportH)
    local self = setmetatable({}, Camera)

    self.body = RigidBody2D.new(x, y)

    self.camera = Camera2D.new()
    if viewportW and viewportH then
        self.camera:SetViewportSize(viewportW, viewportH)
    end
    self.body:SetCamera(self.camera)

    return self
end

-- target: a RigidBody2D to lerp toward every UpdateCamera() call (e.g.
-- player.body). Pass nil to stop following and drive the camera by hand
-- instead (SetPosition, AddForce, whatever). smoothing is optional --
-- higher catches up to the target faster, see Camera2D::followSmoothing.
function Camera:Follow(target, smoothing)
    self.camera:SetFollowTarget(target)
    if smoothing then self.camera:SetFollowSmoothing(smoothing) end
end

-- Call once a frame, AFTER whatever you're following has already moved
-- this frame (so the lerp chases its freshest position, not last frame's).
function Camera:Update(deltaTime)
    self.body:UpdateCamera(deltaTime)
end

return Camera