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
    self.body:SetColor(r or 1.0, g or 1.0, b or 1.0, a or 1.0)
    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2))
    self.body:SetMass(0)

    return self
end

function StaticBody:Draw()
    DrawBody(self.body)
end

return StaticBody