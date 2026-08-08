---@meta
-- Single-file LuaLS/IDE definitions for every symbol ScriptBindings.cpp
-- injects into the Lua environment. Never loaded by the engine at
-- runtime -- IDE/type-checker use only.
--
-- No generator behind this (yet) -- keep it in sync by hand. Sections
-- below are in the same order as ScriptBindings::RegisterAll(), one
-- section per Register* function, so a diff of that function is a
-- checklist of what to update here.

-- =====================================================================
-- Vector2 -- value type (RegisterVector2)
-- =====================================================================
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
---@param x number
function Vector2:SetX(x) end
---@return number
function Vector2:GetY() end
---@param y number
function Vector2:SetY(y) end

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

-- =====================================================================
-- RigidBody2D -- pointer type, owned by ActorRegistry (RegisterRigidBody2D)
-- =====================================================================
---@class RigidBody2D
RigidBody2D = {}

---@param x number
---@param y number
---@param w number|nil Defaults to 50
---@param h number|nil Defaults to 50
---@return RigidBody2D
function RigidBody2D.new(x, y, w, h) end

---@return number x
---@return number y
function RigidBody2D:GetPosition() end
---@param x number
---@param y number
function RigidBody2D:SetPosition(x, y) end

---@return number degrees
function RigidBody2D:GetRotation() end
---@param degrees number
function RigidBody2D:SetRotation(degrees) end

---@param r number
---@param g number
---@param b number
---@param a number|nil Defaults to 1.0
function RigidBody2D:SetColor(r, g, b, a) end

---@return boolean
function RigidBody2D:IsPlayer() end

---@return PixelSprite|nil
function RigidBody2D:GetSprite() end
---@param sprite PixelSprite|nil Also resets draw size to the sprite's native pixel size
function RigidBody2D:SetSprite(sprite) end

---@return number vx
---@return number vy
function RigidBody2D:GetVelocity() end
---@param vx number
---@param vy number
function RigidBody2D:SetVelocity(vx, vy) end

---@return number w
---@return number h
function RigidBody2D:GetSize() end
---@param w number
---@param h number
function RigidBody2D:SetSize(w, h) end

---@return number
function RigidBody2D:GetMass() end
---@param mass number mass <= 0 means immovable/static
function RigidBody2D:SetMass(mass) end

---@return number
function RigidBody2D:GetDrag() end
---@param drag number 0 = no linear damping
function RigidBody2D:SetDrag(drag) end

---@return number degreesPerSecond
function RigidBody2D:GetAngularVelocity() end
---@param degreesPerSecond number
function RigidBody2D:SetAngularVelocity(degreesPerSecond) end

---@return Shader|nil
function RigidBody2D:GetShader() end
---@param shader Shader|nil
function RigidBody2D:SetShader(shader) end

---@return CollisionShape2D|nil
function RigidBody2D:GetCollisionShape() end
---@param shape CollisionShape2D|nil
function RigidBody2D:SetCollisionShape(shape) end

---@return PlayerActorConfig|nil
function RigidBody2D:GetPlayerConfig() end
---@param config PlayerActorConfig|nil
function RigidBody2D:SetPlayerConfig(config) end

--- NOTE: takes a Vector2, not two floats -- the one exception to this
--- file's usual "hot-path values cross as raw numbers" convention.
---@param force Vector2 Accumulated and applied on the next Integrate()
function RigidBody2D:AddForce(force) end

---@param deltaTime number
function RigidBody2D:Integrate(deltaTime) end

---@return boolean grounded True if a resolve call this frame pushed this body up out of an overlap
function RigidBody2D:IsGrounded() end

---@param other RigidBody2D
---@return boolean
function RigidBody2D:CollidesWith(other) end

---@param other RigidBody2D mass <= 0 is treated as immovable
---@return boolean overlapped
function RigidBody2D:ResolveCollisionWith(other) end

---@param windowWidth number
---@param windowHeight number
---@return boolean clamped
function RigidBody2D:ResolveWindowBounds(windowWidth, windowHeight) end

-- =====================================================================
-- Shader -- pointer type, owned by ActorRegistry (RegisterShader)
-- =====================================================================
---@class Shader
Shader = {}

---@param vertexSrc string
---@param fragmentSrc string
---@return Shader
function Shader.new(vertexSrc, fragmentSrc) end

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

---@return number
function Shader:GetOverdrawScale() end
---@param scale number
function Shader:SetOverdrawScale(scale) end

-- =====================================================================
-- CollisionShape2D -- pointer type, owned by ActorRegistry
-- (RegisterCollisionShape2D)
-- =====================================================================
---@class CollisionShape2D
CollisionShape2D = {}

---@param halfWidth number
---@param halfHeight number
---@param offsetX number|nil Defaults to 0
---@param offsetY number|nil Defaults to 0
---@return CollisionShape2D
function CollisionShape2D.NewBox(halfWidth, halfHeight, offsetX, offsetY) end

---@param radius number
---@param offsetX number|nil Defaults to 0
---@param offsetY number|nil Defaults to 0
---@return CollisionShape2D
function CollisionShape2D.NewCircle(radius, offsetX, offsetY) end

---@return string # "Box" or "Circle"
function CollisionShape2D:GetType() end

-- =====================================================================
-- PixelSprite -- pointer type, owned by ActorRegistry (RegisterPixelSprite).
-- Not itself a runtime global -- the real global is `Sprite` below (only
-- exposes .Load). This type-only table exists purely so instance methods
-- have somewhere to attach for the type checker.
-- =====================================================================
---@class PixelSprite
local PixelSprite = {}

---@return integer
function PixelSprite:GetWidth() end
---@return integer
function PixelSprite:GetHeight() end
--- Uploads any pending SetPixel/PunchCircle edits to the GPU texture --
--- call before DrawBody() if you need an edit to show up the same frame.
function PixelSprite:Flush() end
---@param centerX integer
---@param centerY integer
---@param radius number
function PixelSprite:PunchCircle(centerX, centerY, radius) end
---@param x integer
---@param y integer
---@param r number
---@param g number
---@param b number
---@param a number|nil Defaults to 1.0
function PixelSprite:SetPixel(x, y, r, g, b, a) end
---@param x integer
---@param y integer
---@return boolean
function PixelSprite:IsSolid(x, y) end

---@class Sprite
Sprite = {}

---@param filepath string PNG path -- cached by path, repeat Load() calls with the same path return the same PixelSprite
---@return PixelSprite
function Sprite.Load(filepath) end

-- =====================================================================
-- PlayerActorConfig -- pointer type, owned by ActorRegistry
-- (RegisterPlayerActorConfig)
-- =====================================================================
---@class PlayerActorConfig
PlayerActorConfig = {}

---@return PlayerActorConfig
function PlayerActorConfig.new() end

---@return number
function PlayerActorConfig:GetMoveSpeed() end
---@param speed number
function PlayerActorConfig:SetMoveSpeed(speed) end

---@return number
function PlayerActorConfig:GetJumpForce() end
---@param force number
function PlayerActorConfig:SetJumpForce(force) end

---@return boolean
function PlayerActorConfig:IsInputEnabled() end
---@param enabled boolean
function PlayerActorConfig:SetInputEnabled(enabled) end

-- =====================================================================
-- Graphics -- bare globals bound to IGraphicsContext* (RegisterGraphics)
-- =====================================================================

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

-- =====================================================================
-- Window -- eWindow global (RegisterWindow)
-- =====================================================================
---@class EngineWindow
local EngineWindow = {}

---@return integer
function EngineWindow:GetWidth() end
---@return integer
function EngineWindow:GetHeight() end
---@param filepath string PNG path
function EngineWindow:SetIcon(filepath) end

--- The engine's window, bound into every script's global scope.
---@type EngineWindow
eWindow = nil

-- =====================================================================
-- Renderer -- bare global DrawBody(body) (RegisterRenderer)
-- =====================================================================

--- Draws a RigidBody2D's flat-color quad, or -- if it has a PixelSprite
--- attached -- flushes pending pixel edits and draws it textured instead.
---@param body RigidBody2D
function DrawBody(body) end

-- =====================================================================
-- Actors -- ActorRegistry-wide queries (RegisterActorRegistry)
-- =====================================================================
---@class Actors
Actors = {}

---@return RigidBody2D|nil # nil if no body has a PlayerActorConfig attached yet
function Actors.GetPlayer() end

--- Logs every RigidBody2D and what's attached to it to stdout.
function Actors.Dump() end

-- =====================================================================
-- Input -- polling input state (RegisterInput)
-- =====================================================================
---@class Input
---@field MouseLeft integer
---@field MouseRight integer
---@field MouseMiddle integer
Input = {}

---@param keycode integer Raw platform keycode -- see Keys below
---@return boolean
function Input.IsKeyDown(keycode) end
---@param keycode integer
---@return boolean
function Input.IsKeyPressed(keycode) end
---@param keycode integer
---@return boolean
function Input.IsKeyReleased(keycode) end

---@param button integer Use Input.MouseLeft / MouseRight / MouseMiddle
---@return boolean
function Input.IsMouseButtonDown(button) end
---@param button integer
---@return boolean
function Input.IsMouseButtonPressed(button) end
---@param button integer
---@return boolean
function Input.IsMouseButtonReleased(button) end

---@return number
function Input.GetScrollDelta() end

---@return number x
---@return number y
function Input.GetMousePosition() end

---@return integer[] # every keycode that went down this frame -- mainly a debugging aid for finding a key's raw code
function Input.GetKeysPressedThisFrame() end

-- =====================================================================
-- Physics -- engine-wide gravity (RegisterPhysics)
-- =====================================================================
---@class Physics
Physics = {}

---@param x number
---@param y number
function Physics.SetGravity(x, y) end

---@return number x
---@return number y
function Physics.GetGravity() end

-- =====================================================================
-- Keys -- scripts/keycodes.lua, loaded/exposed by KeyMap::LoadAndExposeToLua.
-- Not part of ScriptBindings.cpp, but every script touches it -- same
-- field shape on both platform tables in keycodes.lua, so one class
-- covers both.
-- =====================================================================
---@class Keys
---@field W integer
---@field A integer
---@field S integer
---@field D integer
---@field Up integer
---@field Down integer
---@field Left integer
---@field Right integer
---@field Space integer
---@field Escape integer
---@field Enter integer
---@field Backtick integer
---@field Shift integer
---@field Ctrl integer
Keys = {}