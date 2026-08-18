local Class = require("core.Class")

local StaticBody = Class()

---@param x number
---@param y number
---@param w number
---@param h number
---@param r number|nil Defaults to 1
---@param g number|nil Defaults to 1
---@param b number|nil Defaults to 1
---@param a number|nil Defaults to 1
function StaticBody.new(x, y, w, h, r, g, b, a)
    local self = setmetatable({}, StaticBody)

    self.body = RigidBody2D.new(x, y, w, h)

    -- Solid-fill generated sprite instead of a flat-color quad -- every
    -- basic square is pixel-addressable (SetPixel/PunchCircle/lighting)
    -- by default now, not just PNG-backed props. SetSprite also sizes
    -- the body to the sprite's native pixel size, which is exactly w x h
    -- here, so this is a no-op on top of RigidBody2D.new(x, y, w, h).
    self.sprite = Sprite.NewSolid(w, h, r or 1.0, g or 1.0, b or 1.0, a or 1.0)
    self.body:SetSprite(self.sprite)

    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2))
    self.body:SetMass(0)

    return self
end

function StaticBody:Draw()
    DrawBody(self.body)
end

return StaticBody