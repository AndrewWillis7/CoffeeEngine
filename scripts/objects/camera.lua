-- A RigidBody2D plus a Camera2D as one game object. The body's position IS the
-- camera's world centre; it is never drawn and never collides, it just carries
-- the camera around.

local Class = require("core.Class")

local Camera = Class()

-- viewportW/H are world units visible across the full window, regardless of its
-- pixel size -- the resolution knob. Omit them for Camera2D's own 320x180.
function Camera.new(x, y, viewportW, viewportH)
    local self = setmetatable({}, Camera)

    self.body = RigidBody2D.new(x, y)
    self.body:SetName("Camera")

    self.camera = Camera2D.new()
    if viewportW and viewportH then
        self.camera:SetViewportSize(viewportW, viewportH)
    end
    self.body:SetCamera(self.camera)

    return self
end

-- Pass nil to stop following and drive the camera by hand instead. Higher
-- smoothing catches up faster.
function Camera:Follow(target, smoothing)
    self.camera:SetFollowTarget(target)
    if smoothing then self.camera:SetFollowSmoothing(smoothing) end
end

-- Call once a frame, AFTER the target has moved, so the lerp chases this
-- frame's position rather than last frame's.
function Camera:Update(deltaTime)
    self.body:UpdateCamera(deltaTime)
end

-- Integer zoom-out on top of the viewport passed to new(): 1 is unzoomed, 2
-- shows twice as many texels per axis. Whole numbers only -- see SetZoomOut.
function Camera:SetZoomOut(zoom)
    self.camera:SetZoomOut(zoom)
end

function Camera:GetZoomOut()
    return self.camera:GetZoomOut()
end

return Camera