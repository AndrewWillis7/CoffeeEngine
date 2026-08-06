---@meta
-- Definitions-only file describing the Vector2 class Vector2Bindings.cpp
-- injects. Never loaded by the engine at runtime -- IDE/type-checker use only.

---@class Vector2
---@operator add(Vector2): Vector2
---@operator sub(Vector2): Vector2
---@operator mul(number): Vector2
Vector2 = {}

---@param x number|nil
---@param y number|nil
---@return Vector2
function Vector2.new(x, y) end

---@return number
function Vector2:GetX() end
---@return number
function Vector2:GetY() end

---@param x number
---@param y number
function Vector2:Set(x, y) end

---@return number
function Vector2:Length() end
---@return number
function Vector2:LengthSquared() end

---@return Vector2
function Vector2:Normalized() end

---@param other Vector2
---@return number
function Vector2:Dot(other) end

---@param a Vector2
---@param b Vector2
---@return number
function Vector2.Distance(a, b) end