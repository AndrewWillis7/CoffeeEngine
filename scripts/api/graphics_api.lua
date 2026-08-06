---@meta
-- Definitions-only file describing globals GraphicsBindings.cpp injects.
-- Never loaded by the engine at runtime -- IDE/type-checker use only.

--- Sets the clear color used when the frame buffer is cleared each frame.
---@param r number Red channel, 0.0 - 1.0
---@param g number Green channel, 0.0 - 1.0
---@param b number Blue channel, 0.0 - 1.0
function SetClearColor(r, g, b) end

--- Legacy immediate-mode debug quad path (predates the shader-based
--- Renderer2D/DrawBody pipeline). Still present but not the normal way to
--- draw something -- prefer RigidBody2D + DrawBody for anything gameplay.
---@param x number
---@param y number
---@param width number
---@param height number
---@param rotationDegrees number|nil
---@param r number
---@param g number
---@param b number
---@param a number|nil
function DrawDebugQuad(x, y, width, height, rotationDegrees, r, g, b, a) end