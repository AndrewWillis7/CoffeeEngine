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
engineWindow = nil