---@meta
--  Definitions-only file describing globals of Coffee injects from C

--- Sets the clear color used when the frame buffer is cleared each frame
---@param r number Red channel, 0.0 - 1.0
---@param g number Green channel, 0.0 - 1.0
---@param b number Blue channel, 0..0 - 1.0
function SetClearColor(r, g, b) end

---@class EngineWindow
local EngineWindow = {}

--- Current Window width in pixels
---@return integer
function EngineWindow:GetWidth() end

--- Current Window Height in pixels
---@return integer
function EngineWindow:GetHeight() end

--- Sets the window icon from an image file path (PNG)
---@param filepath string
function EngineWindow:SetIcon(filepath) end

--- The engine's window, bound into every script's global scope
---@type EngineWindow
eWindow = nil

---@class RigidBody2D
local RigidBody2D = {}

--- Creates a new physics body with a drawable square. Origin is top-left.
---@param x number
---@param y number
---@param width number
---@param height number
---@return RigidBody2D
function RigidBody2D.new(x, y, width, height) end

---@param fx number
---@param fy number
function RigidBody2D:AddForce(fx, fy) end

--- Advances velocity/position/rotation by dt. Call once per Update.
---@param dt number
function RigidBody2D:Integrate(dt) end

---@return number x, number y
function RigidBody2D:GetPosition() end
---@param x number
---@param y number
function RigidBody2D:SetPosition(x, y) end

--- Degrees.
---@return number
function RigidBody2D:GetRotation() end
---@param degrees number
function RigidBody2D:SetRotation(degrees) end

---@return number x, number y
function RigidBody2D:GetVelocity() end
---@param x number
---@param y number
function RigidBody2D:SetVelocity(x, y) end

--- Degrees/sec.
---@return number
function RigidBody2D:GetAngularVelocity() end
---@param degreesPerSec number
function RigidBody2D:SetAngularVelocity(degreesPerSec) end

---@param width number
---@param height number
function RigidBody2D:SetSize(width, height) end

---@param r number
---@param g number
---@param b number
---@param a number|nil
function RigidBody2D:SetColor(r, g, b, a) end

---@param mass number
function RigidBody2D:SetMass(mass) end
---@param drag number
function RigidBody2D:SetDrag(drag) end

--- Pass a Shader, or nil to revert to the default flat-color shader.
---@param shader Shader|nil
function RigidBody2D:SetShader(shader) end

---@class Shader
local Shader = {}

--- Compiles a custom GLSL 120 vertex+fragment shader pair.
---@param vertexSource string
---@param fragmentSource string
---@return Shader
function Shader.new(vertexSource, fragmentSource) end

--- The engine's built-in glow effect. Customize with SetVec3/SetFloat below.
---@return Shader
function Shader.CreateGlow() end

---@param name string
---@param value number
function Shader:SetFloat(name, value) end
---@param name string
---@param x number
---@param y number
function Shader:SetVec2(name, x, y) end
---@param name string
---@param x number
---@param y number
---@param z number
function Shader:SetVec3(name, x, y, z) end
---@param name string
---@param x number
---@param y number
---@param z number
---@param w number
function Shader:SetVec4(name, x, y, z, w) end

--- How much bigger than the body's own size to draw the quad (for glow bleed).
---@param scale number
function Shader:SetOverdrawScale(scale) end

--- Draws a RigidBody2D through the shader pipeline (uses its attached
--- shader, or the default flat shader if none was set).
---@param body RigidBody2D
function DrawBody(body) end