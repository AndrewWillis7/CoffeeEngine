local Class = require("core.Class")

local Prop = Class()

---@param x number
---@param y number
---@param w number
---@param h number
---@param spritePath string|nil PNG path; replaces the generated solid-color sprite
---@param r number|nil Fill color if no spritePath given. Defaults to 1
---@param g number|nil Fill color if no spritePath given. Defaults to 1
---@param b number|nil Fill color if no spritePath given. Defaults to 1
---@param a number|nil Fill color if no spritePath given. Defaults to 1
function Prop.new(x, y, w, h, spritePath, r, g, b, a)
    local self = setmetatable({}, Prop)

    self.body = RigidBody2D.new(x, y, w, h)
    self.body:SetName("Prop")
    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2))
    self.body:SetMass(1)

    -- A generated sprite rather than a flat quad, so every Prop is
    -- pixel-addressable, not just the PNG-backed ones.
    if spritePath then
        self.sprite = Sprite.Load(spritePath)
    else
        self.sprite = Sprite.NewSolid(w, h, r or 1.0, g or 1.0, b or 1.0, a or 1.0)
    end
    self.body:SetSprite(self.sprite)

    return self
end

-- solids is an array of RigidBody2D to resolve against this frame.
-- worldWidth/worldHeight are texels, not real window pixels.
function Prop:Update(deltaTime, solids, worldWidth, worldHeight)
    self.body:Integrate(deltaTime)
    self.body:ResolveWindowBounds(worldWidth or eWindow:GetWidth(), worldHeight or eWindow:GetHeight())
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