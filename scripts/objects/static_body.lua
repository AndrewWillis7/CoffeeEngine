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

    -- A generated sprite rather than a flat quad, so every basic square is
    -- pixel-addressable by default, not just PNG-backed props.
    self.sprite = Sprite.NewSolid(w, h, r or 1.0, g or 1.0, b or 1.0, a or 1.0)
    self.body:SetSprite(self.sprite)

    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2))
    self.body:SetMass(0)

    return self
end

function StaticBody:Draw()
    DrawBody(self.body)
end

--- Resolves `body` out of this one, box-vs-box. Exists so a level can keep ONE
--- `solids` list of StaticBodys and Terrains and call them all the same way --
--- terrain resolves as a heightmap through an entirely different path, and
--- nothing that walks around should have to know which it is standing on.
--- @param body userdata RigidBody2D to push out
function StaticBody:ResolveAgainst(body)
    body:ResolveCollisionWith(self.body)
end

return StaticBody