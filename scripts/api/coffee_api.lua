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

--- Standard EWMH fullscreen toggle on Linux (borderless-fullscreen on
--- Windows) -- stays correctly letterboxed/pillarboxed at any size,
--- never stretches. See main.lua's F11 handler for the usual call site.
---@param fullscreen boolean
function EngineWindow:SetFullscreen(fullscreen) end
---@return boolean
function EngineWindow:IsFullscreen() end

--- The engine's window, bound into every script's global scope.
---@type EngineWindow
eWindow = nil

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

--- Per-object visual multiplier on top of GetSize()/SetSize()'s logical/
--- collision size -- deliberately decoupled, so scaling a sprite up/down
--- visually never silently resizes its CollisionShape2D underneath it.
---@return number sx
---@return number sy
function RigidBody2D:GetScale() end
---@param sx number
---@param sy number|nil Defaults to sx -- body:SetScale(2) means uniform 2x
function RigidBody2D:SetScale(sx, sy) end

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

--- When set, this body IS a camera -- Actors.GetActiveCamera() will find
--- it (if the Camera2D is active) and Renderer2D maps world-space draws
--- relative to this body's position and the Camera2D's viewportSize.
---@return Camera2D|nil
function RigidBody2D:GetCamera() end
---@param camera Camera2D|nil
function RigidBody2D:SetCamera(camera) end

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

--- If a Camera2D is attached, lerps this body toward followTarget (plus
--- focusOffset). No-op otherwise. Call once a frame, AFTER whatever it's
--- following has already moved this frame.
---@param deltaTime number
function RigidBody2D:UpdateCamera(deltaTime) end

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
-- Renderer -- bare globals DrawBody(body) / SyncCamera() (RegisterRenderer)
-- =====================================================================

--- Draws a RigidBody2D's flat-color quad, or -- if it has a PixelSprite
--- attached -- flushes pending pixel edits and draws it textured instead.
---@param body RigidBody2D
function DrawBody(body) end

--- Resolves ActorRegistry's currently-active camera (see
--- Actors.GetActiveCamera()) and pushes it into the renderer for the
--- rest of this frame's world-space draws -- including its targetAspect,
--- the "Border" named shader, and any attached border sprite (see
--- Actors.SetBorderSprite), so the letterbox/pillarbox margins get
--- whatever border effect is currently loaded -- or clears it if no
--- camera is active. Call once per frame (after any camera-follow
--- update, before your DrawBody() calls) -- deliberately NOT automatic
--- inside DrawBody() itself, which would re-resolve the active camera on
--- every single object drawn.
function SyncCamera() end

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
-- Camera2D -- pointer type, owned by ActorRegistry (RegisterCamera2D)
-- =====================================================================
---@class Camera2D
Camera2D = {}

---@return Camera2D
function Camera2D.new() end

---@return number w
---@return number h
function Camera2D:GetViewportSize() end
---@param w number World units visible across the full window -- see Camera2D.h's "resolution control" comment
---@param h number
function Camera2D:SetViewportSize(w, h) end

---@return number w
---@return number h
function Camera2D:GetTargetAspect() end
---@param w number Zero (either component) means "not set" -- fit directly to the window using viewportSize's own aspect
---@param h number
function Camera2D:SetTargetAspect(w, h) end

---@return number x
---@return number y
function Camera2D:GetFocusOffset() end
---@param x number World-space offset from the follow target the camera actually leans toward -- e.g. (0, -40) frames 40px above the target. Follows "+y is down".
---@param y number
function Camera2D:SetFocusOffset(x, y) end

---@return RigidBody2D|nil
function Camera2D:GetFollowTarget() end
---@param target RigidBody2D|nil nil stops following -- drive the camera body's position by hand instead
function Camera2D:SetFollowTarget(target) end

---@return number
function Camera2D:GetFollowSmoothing() end
---@param perSecond number Exponential-decay follow rate, not a 0..1 blend -- 0 disables auto-follow
function Camera2D:SetFollowSmoothing(perSecond) end

---@return boolean
function Camera2D:IsActive() end
---@param active boolean Only one active camera drives rendering at a time -- see Actors.GetActiveCamera()
function Camera2D:SetActive(active) end

---@return integer
function Camera2D:GetZoomOut() end
---@param zoom integer Multiplies viewportSize when framing the world (1 = unzoomed). Clamped to >= 1. Kept a whole number so every texel always scales by the same on-screen amount as its neighbors -- see Camera2D.h.
function Camera2D:SetZoomOut(zoom) end

-- =====================================================================
-- Actors -- ActorRegistry-wide queries (RegisterActorRegistry)
-- =====================================================================
---@class Actors
Actors = {}

---@return RigidBody2D|nil # nil if no body has a PlayerActorConfig attached yet
function Actors.GetPlayer() end

---@return RigidBody2D|nil # nil if no body has an active Camera2D attached yet
function Actors.GetActiveCamera() end

--- Returns the cached instance for `name`, compiling it from
--- ShaderLibrary the first time it's requested. Built-in names: "Glow",
--- "RoundedPanel", "Textured", "Text", "Border".
---@param name string
---@return Shader|nil # nil if nothing is registered under that name
function Actors.GetNamedShader(name) end

--- Reads a .frag file off disk (paired with the engine's shared vertex
--- stage, scripts/shaders/quad.vert) and installs it under `name`,
--- REPLACING whatever's currently cached there. Also swappable at
--- runtime this way for e.g. "Border" -- see scripts/shaders/*.frag.
---@param name string
---@param fragmentPath string
---@return boolean success
function Actors.LoadShaderFromFile(name, fragmentPath) end

--- Attaches a PixelSprite behind the letterbox/pillarbox margins,
--- drawn by SyncCamera() alongside the "Border" named shader whenever
--- that shader declares `uniform sampler2D u_Texture`. nil for no
--- texture (procedural border only, the default).
---@param sprite PixelSprite|nil
function Actors.SetBorderSprite(sprite) end

---@return PixelSprite|nil
function Actors.GetBorderSprite() end

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
---@field F11 integer
Keys = {}