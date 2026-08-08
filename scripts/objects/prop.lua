local Class = require("core.Class")

local Prop = Class()

---@param x number
---@param y number
---@param w number
---@param h number
---@param spritePath string|nil PNG path -- if given, replaces the flat-color quad with this texture
function Prop.new(x, y, w, h, spritePath)
    local self = setmetatable({}, Prop)

    self.body = RigidBody2D.new(x, y, w, h)
    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2))
    self.body:SetMass(1)

    if spritePath then
        self.sprite = Sprite.Load(spritePath)
        self.body:SetSprite(self.sprite)
    end

    return self
end

-- solids: array of RigidBody2D to resolve collisions against this frame,
-- same convention Player:Update() already uses.
function Prop:Update(deltaTime, solids)
    self.body:Integrate(deltaTime)
    self.body:ResolveWindowBounds(eWindow:GetWidth(), eWindow:GetHeight())
    if solids then
        for _, solid in ipairs(solids) do
            self.body:ResolveCollisionWith(solid)
        end
    end
end

function Prop:Draw()
    DrawBody(self.body)
end

return Prop