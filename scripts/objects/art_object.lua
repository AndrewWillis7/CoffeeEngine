-- A non-colliding drawable: follows position, rotation and scale, but carries
-- no collider and is never integrated, so physics never touches it. For
-- backgrounds, parallax layers, decoration -- anything that sits there and
-- looks like something without joining in.
--
-- Underneath it is still just a RigidBody2D with nothing attached; there is no
-- dedicated C++ type. The wrapper exists for readability at the call site, the
-- way StaticBody.new(...) says "collides but never moves".

local Class = require("core.Class")

local ArtObject = Class()

---@param x number
---@param y number
---@param w number
---@param h number
---@param spritePath string|nil PNG path; replaces the generated solid-color sprite
---@param r number|nil Fill color if no spritePath given. Defaults to 1
---@param g number|nil Fill color if no spritePath given. Defaults to 1
---@param b number|nil Fill color if no spritePath given. Defaults to 1
---@param a number|nil Fill color if no spritePath given. Defaults to 1
function ArtObject.new(x, y, w, h, spritePath, r, g, b, a)
    local self = setmetatable({}, ArtObject)

    self.body = RigidBody2D.new(x, y, w, h)

    if spritePath then
        self.sprite = Sprite.Load(spritePath)
    else
        self.sprite = Sprite.NewSolid(w, h, r or 1.0, g or 1.0, b or 1.0, a or 1.0)
    end
    self.body:SetSprite(self.sprite)

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