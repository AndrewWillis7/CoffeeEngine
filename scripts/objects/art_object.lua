-- Generic, non-colliding drawable: follows position/rotation/scale, but
-- carries no CollisionShape2D and is never Integrate()'d -- so physics
-- and collision never touch it. For backgrounds, parallax layers,
-- decoration, signage, anything that just needs to sit somewhere and
-- look like something without participating in gameplay.
--
-- Under the hood this is still "just" a RigidBody2D (same as every other
-- drawable in the engine) with nothing attached to it -- there's no
-- dedicated C++ type here, matching PlayerActorConfig/CollisionShape2D
-- being OPT-IN attachments rather than every RigidBody2D always paying
-- for them. This wrapper exists purely for Lua-side clarity/ergonomics:
-- reading `ArtObject.new(...)` at a call site tells you at a glance
-- "this thing doesn't simulate", the same way `StaticBody.new(...)` tells
-- you "this collides but never moves".

local Class = require("core.Class")

local ArtObject = Class()

---@param x number
---@param y number
---@param w number
---@param h number
---@param spritePath string|nil PNG path -- if given, replaces the flat-color quad with this texture
function ArtObject.new(x, y, w, h, spritePath)
    local self = setmetatable({}, ArtObject)

    self.body = RigidBody2D.new(x, y, w, h)

    if spritePath then
        self.sprite = Sprite.Load(spritePath)
        self.body:SetSprite(self.sprite)
    end

    return self
end

function ArtObject:SetPosition(x, y)
    self.body:SetPosition(x, y)
end

function ArtObject:GetPosition()
    return self.body:GetPosition()
end

-- sy defaults to sx if omitted -- SetScale(2) means uniform 2x.
function ArtObject:SetScale(sx, sy)
    self.body:SetScale(sx, sy)
end

function ArtObject:GetScale()
    return self.body:GetScale()
end

function ArtObject:SetRotation(degrees)
    self.body:SetRotation(degrees)
end

function ArtObject:SetColor(r, g, b, a)
    self.body:SetColor(r, g, b, a)
end

function ArtObject:Draw()
    DrawBody(self.body)
end

return ArtObject