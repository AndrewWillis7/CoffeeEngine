local Class = require("core.Class")

local Prop = Class()

---@param x number
---@param y number
---@param w number
---@param h number
---@param spritePath string|nil PNG path -- if given, replaces the generated solid-color sprite with this texture
---@param r number|nil Fill color if no spritePath given. Defaults to 1
---@param g number|nil Fill color if no spritePath given. Defaults to 1
---@param b number|nil Fill color if no spritePath given. Defaults to 1
---@param a number|nil Fill color if no spritePath given. Defaults to 1
function Prop.new(x, y, w, h, spritePath, r, g, b, a)
    local self = setmetatable({}, Prop)

    self.body = RigidBody2D.new(x, y, w, h)
    self.body:SetCollisionShape(CollisionShape2D.NewBox(w / 2, h / 2))
    self.body:SetMass(1)

    -- Every Prop is pixel-addressable now, not just PNG-backed ones --
    -- with no spritePath given, this generates a solid-fill sprite
    -- instead of falling back to a flat-color quad (see Sprite.NewSolid).
    if spritePath then
        self.sprite = Sprite.Load(spritePath)
    else
        self.sprite = Sprite.NewSolid(w, h, r or 1.0, g or 1.0, b or 1.0, a or 1.0)
    end
    self.body:SetSprite(self.sprite)

    return self
end

-- solids: array of RigidBody2D to resolve collisions against this frame,
-- same convention Player:Update() already uses. worldWidth/worldHeight:
-- see Player:Update's comment -- texels, not real window pixels.
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