---@meta
-- Definitions-only file describing the eWindow global WindowBindings.cpp
-- injects. Never loaded by the engine at runtime -- IDE/type-checker use only.

---@class EngineWindow
local EngineWindow = {}

--- Current window width in pixels.
---@return integer
function EngineWindow:GetWidth() end

--- Current window height in pixels.
---@return integer
function EngineWindow:GetHeight() end

--- Sets the window icon from an image file path (PNG).
---@param filepath string
function EngineWindow:SetIcon(filepath) end

--- The engine's window, bound into every script's global scope.
---@type EngineWindow
eWindow = nil